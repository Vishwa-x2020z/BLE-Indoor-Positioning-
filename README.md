# 📡 BLE Indoor Positioning System

> Real-time indoor location tracking using BLE beacons, RSSI signal mapping, and zone-based detection — built on Raspberry Pi with live 2D visualization.

---

## 🔍 Overview

This project implements an **indoor positioning system** that scans for BLE (Bluetooth Low Energy) beacons, processes their signal strength (RSSI), and maps device locations to a floor layout in real time. It was developed and deployed at Intelense as part of a production-grade tracking solution.

**Key achievement:** Sub-2m location accuracy across 10+ zones using adaptive RSSI filtering.

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| Hardware | Raspberry Pi 4, BLE Beacons |
| Language | C, Python |
| Protocols | BLE (GATT/GAP), TCP/UDP |
| Libraries | BlueZ, Socket programming |
| Visualization | Python (Matplotlib / custom 2D dashboard) |
| OS | Embedded Linux (Raspberry Pi OS) |

---

## ⚙️ How It Works

```
BLE Beacons (UUID + MAC + RSSI)
        │
        ▼
  HCI Scanner (BlueZ / C)
        │
        ▼
  RSSI Filtering & Averaging
  (Kalman / Weighted Moving Avg)
        │
        ▼
  Zone Classification Engine
        │
        ▼
  TCP/UDP Data Pipeline ──► Database Storage
        │
        ▼
  Real-time 2D Floor Map Visualization
```

---

## 📂 Project Structure

```
ble-indoor-positioning/
├── src/
│   ├── scanner.c          # BLE HCI scanning (BlueZ)
│   ├── rssi_filter.c      # RSSI signal processing & filtering
│   ├── zone_mapper.c      # Zone classification logic
│   └── tcp_server.c       # TCP/UDP data pipeline
├── python/
│   ├── visualizer.py      # Real-time 2D floor map dashboard
│   └── db_handler.py      # Database interface
├── config/
│   └── floor_layout.json  # Zone coordinates & beacon positions
├── Makefile
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites
```bash
sudo apt update
sudo apt install bluez libbluetooth-dev python3-matplotlib
```

### Build & Run
```bash
# Build the C scanner
make all

# Start the BLE scanner (requires root for HCI access)
sudo ./bin/ble_scanner

# Launch the visualization dashboard
python3 python/visualizer.py
```

---

## 📊 Results

- ✅ **Accuracy:** Sub-2m positioning in 10+ distinct zones
- ✅ **Latency:** ~40% lower than polling-based alternatives
- ✅ **Beacons supported:** Multiple simultaneous UUID/MAC identifiers
- ✅ **Real-time:** Live 2D map updates at ~1s refresh rate

---

## 📸 Demo

> *(Add a screenshot or short GIF of the 2D visualization dashboard here)*

---

## 🔑 Key Concepts

- **RSSI filtering** — Weighted moving average + Kalman filtering to smooth noisy BLE signal data
- **Zone mapping** — Signal-strength thresholds mapped to physical floor zones
- **Multi-beacon fusion** — Triangulation using multiple beacons for improved accuracy
- **TCP/UDP pipeline** — Real-time data streaming from embedded device to dashboard

---

## 👤 Author

**Vishwanath Goroshi** — Embedded Engineer Intern @ Intelense  
📧 vishwa.goroshi@gmail.com | 📍 Bangalore, India
