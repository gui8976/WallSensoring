# WallSensoring

This repository is one of the projects integrated into my master's thesis. It implements a lightweight structural health monitoring (SHM) pipeline for a wall, collecting sensor data (temperature, humidity, and strain/extensometer readings) and pushing it through a three-layer architecture into a database for visualization.

## Architecture Overview

The system is designed around three main considerations: the communication protocol, the layered structure of the pipeline, and the role each device plays within it.

### Communication Protocol

Communication between the ESP32 nodes and the Raspberry Pi is done over **MQTT**, chosen over a raw TCP-based approach for being lighter-weight and better suited to constrained, battery-powered microcontrollers publishing small, frequent payloads.

### Layers

The pipeline is organized into three layers:

- **Top layer — HMI (Human-Machine Interface):** Responsible for user interaction with the collected data. This can be a custom program reading directly from the database, or, as in this project, **Grafana** — a widely used industrial dashboarding tool — connected to the database.
- **Middle layer — Broker & Database:** A single Raspberry Pi 5 that hosts the MQTT broker and the database, and also manages the battery/power side of the system.
- **Bottom layer — Sensing nodes:** ESP32 boards with attached sensors that collect data from the wall and publish it via MQTT to the Raspberry Pi 5.

```
[ ESP32 + sensors ]  --MQTT-->  [ Raspberry Pi 5: broker + database ]  -->  [ Grafana / HMI ]
   bottom layer                         middle layer                        top layer
```

## Repository Structure

```
WallSensoring/
├── Rasp5/
│   ├── broker.py       # MQTT broker running on the Raspberry Pi 5
│   ├── database.py     # MQTT subscriber that writes sensor readings into SQLite
│   └── receiver.py     # Lightweight MQTT subscriber (in-memory only, no DB write)
├── esp32/
│   └── main.cpp         # Arduino (Nano ESP32) firmware: reads sensors, publishes over MQTT
└── sensorArduino/       # Individual sensor test sketches (moisture, temperature, humidity)
```

### Rasp5/broker.py
Starts the MQTT broker itself (via `amqtt`) on port `1883`, and runs a listener that subscribes to all topics (`#`) and logs incoming messages — mainly useful for debugging what is arriving from the ESP32 nodes.

### Rasp5/database.py
Subscribes to the `esp32/sensors` topic and persists incoming readings to a local **SQLite** database (`sensor_data.db`). Each device gets its own table (named from its `device_id`), and readings are buffered in memory and flushed to disk on an interval to reduce write load.

### Rasp5/receiver.py
A simpler subscriber, functionally similar to `database.py`, but it only keeps readings in memory (`sensor_data_history`) and prints them — no persistence. Useful for quick testing without touching the database.

### esp32/main.cpp
Firmware for the ESP32 node (Arduino Nano ESP32, written in Arduino C++). It connects to Wi-Fi and to the MQTT broker, then every 2 seconds reads the temperature, humidity, and extensometer sensors, packages the values as JSON, and publishes them to the `esp32/sensorReadings` topic.

## Data Flow

1. Each ESP32 node reads its sensors and publishes a JSON payload (`device_id`, sensor values, timestamp) over MQTT.
2. The Raspberry Pi 5, running `broker.py`, receives the message via the MQTT broker.
3. `database.py` subscribes to the sensor topic and writes each reading into its device's SQLite table (or `receiver.py` can be used instead for in-memory-only testing).
4. Grafana (or another HMI) reads from the database to visualize the data.

## Status

This is an active work-in-progress project, part of an ongoing master's thesis on structural health monitoring.