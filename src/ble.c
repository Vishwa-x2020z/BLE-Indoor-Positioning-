#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <sys/socket.h>
#include <sqlite3.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

volatile sig_atomic_t stop = 0;
void sigint_handler(int sig){ stop = 1; }

// Kalman Filter structure
typedef struct {
    double Q;
    double R;
    double X;
    double P;
    double K;
} KalmanFilter;

void kalman_init(KalmanFilter *kf, double process_noise, double measurement_noise, double initial_value) {
    kf->Q = process_noise;
    kf->R = measurement_noise;
    kf->X = initial_value;
    kf->P = 1.0;
    kf->K = 0.0;
}

double kalman_update(KalmanFilter *kf, double measurement) {
    kf->P = kf->P + kf->Q;
    kf->K = kf->P / (kf->P + kf->R);
    kf->X = kf->X + kf->K * (measurement - kf->X);
    kf->P = (1 - kf->K) * kf->P;
    return kf->X;
}

// Distance calculation
double rssi_to_distance(int rssi, int txPower, double n) {
    return pow(10.0, ((double)(txPower - rssi)) / (10.0 * n));
}

typedef struct {
    char mac[18];
    KalmanFilter kf;
    char id; // 'A', 'B', 'C' etc
} target_t;

// ✅ Function to insert data into SQLite database
void insert_into_db(char id, double distance) {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open("/home/beacon/C_Projects/beacon_map/distance.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS DISTANCE_LOG ("
        "id CHAR(1), "
        "distance REAL, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    rc = sqlite3_exec(db, sql_create, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }

    char sql_insert[256];
    snprintf(sql_insert, sizeof(sql_insert),
             "INSERT INTO DISTANCE_LOG (id, distance) VALUES ('%c', %.2f);",
             id, distance);

    rc = sqlite3_exec(db, sql_insert, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Insert error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    sqlite3_close(db);
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <MAC1> [MAC2 ...]\n", argv[0]);
        return 1;
    }

    int n_targets = argc - 1;
    target_t *targets = calloc(n_targets, sizeof(target_t));
    if(!targets){
        perror("calloc");
        return 1;
    }

    int txPower = -45; // RSSI at 1 meter
    double n = 2.3;    // Path loss exponent

    for(int i = 0; i < n_targets; ++i){
        strncpy(targets[i].mac, argv[i+1], sizeof(targets[i].mac)-1);
        targets[i].mac[sizeof(targets[i].mac)-1] = '\0';
        targets[i].id = 'A' + i;  // assign A, B, C, ...
        kalman_init(&targets[i].kf, 0.125, 8.0, txPower);
    }

    signal(SIGINT, sigint_handler);

    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if(dev_id < 0 || sock < 0){
        perror("hci_open_dev");
        free(targets);
        return 1;
    }

    if(hci_le_set_scan_parameters(sock, 0x00, htobs(0x10), htobs(0x10), 0x00, 0x00, 1000) < 0){
        perror("Set scan parameters failed");
        close(sock);
        free(targets);
        return 1;
    }
    if(hci_le_set_scan_enable(sock, 0x01, 0x00, 1000) < 0){
        perror("Enable scan failed");
        close(sock);
        free(targets);
        return 1;
    }

    struct hci_filter nf, of;
    socklen_t olen = sizeof(of);
    getsockopt(sock, SOL_HCI, HCI_FILTER, &of, &olen);
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    setsockopt(sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));

    unsigned char buf[HCI_MAX_EVENT_SIZE];
    char addr[18];
    int len;

    printf("Scanning for %d BLE device(s)... Press Ctrl+C to stop.\n", n_targets);

    const int RSSI_MIN = -90;  // Ignore RSSI below this threshold

    while(!stop){
        len = read(sock, buf, sizeof(buf));
        if(len < 0){
            if(errno == EINTR) continue;
            perror("read");
            break;
        }

        evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
        if(meta->subevent != EVT_LE_ADVERTISING_REPORT) continue;

        le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
        ba2str(&info->bdaddr, addr);

        for(int i = 0; i < n_targets; ++i){
            if(strcasecmp(addr, targets[i].mac) == 0){
                int8_t rssi = (int8_t)buf[len - 1];

                // Ignore invalid or too weak signals
                if(rssi < RSSI_MIN) continue;

                double filtered_rssi = kalman_update(&targets[i].kf, rssi);
                double distance = rssi_to_distance(filtered_rssi, txPower, n);

                time_t now = time(NULL);
                char buf_time[64];
                strftime(buf_time, sizeof(buf_time), "%Y-%m-%d %H:%M:%S", localtime(&now)); // ✅ 24-hour format

                printf("%s | ID: %c | MAC: %s | RSSI: %d dBm | Filtered: %.2f | Distance: %.2f m\n",
                       buf_time, targets[i].id, targets[i].mac, rssi, filtered_rssi, distance);
                fflush(stdout);

                // ✅ Store in SQLite instead of .txt file
                insert_into_db(targets[i].id, distance);
                break;
            }
        }
    }

    hci_le_set_scan_enable(sock, 0x00, 0x00, 1000);
    setsockopt(sock, SOL_HCI, HCI_FILTER, &of, sizeof(of));
    close(sock);
    free(targets);
    printf("Stopped.\n");

    return 0;
}