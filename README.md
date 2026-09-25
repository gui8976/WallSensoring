# WallSensoring
 
Lightweight SHM pipeline for a wall: ESP32 nodes collect temperature, humidity, and strain/extensometer readings and push them through MQTT into a database for visualization.
 
> Full architecture rationale, protocol choice, and data flow details are in the [wiki](../../wiki).
 
## Architecture
 
```
[ ESP32 + sensors ]  --MQTT-->  [ Raspberry Pi 5: broker + database ]  -->  [ Grafana / HMI ]
   bottom layer                         middle layer                        top layer
```
 
- **Bottom:** ESP32 nodes (Arduino Nano ESP32, C++) read sensors, publish JSON over MQTT
- **Middle:** Raspberry Pi 5 hosts MQTT broker + SQLite database
- **Top:** Grafana dashboards reading from the database
## Repository Structure
 
```
WallSensoring/
├── Rasp5/
│   ├── broker.py       # MQTT broker
│   ├── NEW_BASE.py     # Subscriber → writes readings to SQLite
├── esp32/
│   └── main.cpp         # An example for the ESP32 firmware: reads sensors, publishes over MQTT, concrete information is on the subsquent folders
└── sensorArduino/       # Individual sensor test sketches
```
 
## Status
Active work-in-progress, part of an ongoing SHM research project.
