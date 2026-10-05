# WallSensoring
 
Lightweight SHM pipeline for a wall: ESP32 nodes collect temperature, humidity, and strain/extensometer readings and push them through MQTT into a database for visualization.
 
> Full architecture rationale, protocol choice, and data flow details are in the [wiki](../../wiki).
 
## Architecture
 
<img width="2488" height="2168" alt="system_architecture" src="https://github.com/user-attachments/assets/b3358661-8764-4faa-8033-7947e77a12dd" />

 
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
 
