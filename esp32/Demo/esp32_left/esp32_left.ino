#include <WiFi.h>
#include <PubSubClient.h>

// ==============================================================================
// CONFIGURATION
// ==============================================================================

// Wi-Fi Credentials
const char* WIFI_SSID     = "Gui's Pixel 9";
const char* WIFI_PASSWORD = "gaiusjullius";

// MQTT Broker Settings (PC/Pi IP running broker.py)
const char* MQTT_SERVER   = "10.40.31.167";
const int   MQTT_PORT     = 1883;


const char* MQTT_USER     = nullptr;
const char* MQTT_PASS     = nullptr;


// Topics & Identifiers
const char* MQTT_TOPIC    = "esp32/sensorReadings";
const char* CLIENT_ID     = "ESP32_left";  // Must be unique per ESP32

// ==============================================================================
// SENSOR SETUP — this board carries 3 physical sensors:
//   1) DFR0028 - Digital Tilt Sensor (digital HIGH/LOW output)
//   2) DFR0026 - Analog Ambient Light Sensor (analog voltage output)
//   3) SEN0114 - Analog Soil Moisture Sensor (analog voltage output)
// None of these need an external library - plain digitalRead/analogRead.
// ==============================================================================
const int PIN_TILT      = D2;  // DFR0028 - digital pin, HIGH/LOW
const int PIN_LIGHT     = A0;  // DFR0026 - analog pin (ADC1)
const int PIN_MOISTURE  = A1;  // SEN0114 - analog pin (ADC1)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 5000; // 5 seconds

// ==============================================================================
// SENSOR CALIBRATION CONSTANTS
// ==============================================================================

// SEN0114 moisture sensor (raw 12-bit ADC values)
const int DRY_VALUE = 0;      // sensor in air   -> 0%
const int WET_VALUE = 3000;   // sensor in water -> 100%

// DFR0026 light sensor (millivolts)
const int NUM_SAMPLES = 16;   // readings averaged per measurement
const int DARK_MV     = 100;  // sensor covered       -> 0%
const int BRIGHT_MV   = 3000; // sensor in strong light -> 100%

// ==============================================================================
// SENSOR READING FUNCTIONS
// ==============================================================================

// SEN0114: raw ADC -> 0-100 % (dry = low, wet = high)
int readMoisturePercent() {
  int rawValue = analogRead(PIN_MOISTURE);
  int percent = map(rawValue, DRY_VALUE, WET_VALUE, 0, 100);
  return constrain(percent, 0, 100);
}

// DFR0026: averaged millivolts -> 0-100 % (relative light level, not lux)
int readLightPercent() {
  long sumMv = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sumMv += analogReadMilliVolts(PIN_LIGHT);
    delay(2);
  }
  int mV = sumMv / NUM_SAMPLES;
  int percent = map(mV, DARK_MV, BRIGHT_MV, 0, 100);
  return constrain(percent, 0, 100);
}

// DFR0028: 0 or 1
int readTilt() {
  return digitalRead(PIN_TILT);
}

// ==============================================================================
// HELPER FUNCTIONS
// ==============================================================================

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi '");
  Serial.print(WIFI_SSID);
  Serial.println("'...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("[Wi-Fi] Connected successfully!");
  Serial.print("[Wi-Fi] ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  while (!client.connected()) {
    Serial.print("[MQTT] Connecting to broker at ");
    Serial.print(MQTT_SERVER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    if (client.connect(CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] Successfully connected to broker!");
    } else {
      Serial.print("[MQTT Error] Failed to connect, rc=");
      Serial.print(client.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    ; // Wait up to 3 s for the serial port, so the board also runs without USB
  }
  Serial.println("[System] Starting...");

  analogReadResolution(12); // 0-4095
  pinMode(PIN_TILT, INPUT);

  connectWiFi();
  connectMQTT();

  Serial.println("[System] Starting continuous sensor monitoring loop...");
}

// ==============================================================================
// PUBLISH HELPER — sends ONE sensor reading per MQTT message, even though
// several sensors live on this same board.
// ==============================================================================

void publishReading(const char* sensorKey, float value, unsigned long timestamp) {
  char payload[150];
  snprintf(payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"%s\":%.2f,\"timestamp_ms\":%lu}",
    CLIENT_ID, sensorKey, value, timestamp);

  Serial.print("[Publishing] Topic: ");
  Serial.print(MQTT_TOPIC);
  Serial.print(" | Payload: ");
  Serial.println(payload);

  client.publish(MQTT_TOPIC, payload);
}

// ==============================================================================
// MAIN LOOP
// ==============================================================================

void loop() {
  if (!client.connected()) {
    Serial.println("[System Error] Lost MQTT connection. Attempting to reconnect...");
    connectMQTT();
    Serial.println("[System] Reconnected to MQTT broker successfully!");
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    // Step A: Read the 3 sensors (already converted to final units)
    int tilt        = readTilt();             // 0 or 1
    int lightPct    = readLightPercent();     // 0-100 %
    int moisturePct = readMoisturePercent();  // 0-100 %

    // Step B: Publish each reading as its own message (same timestamp for the cycle)
    publishReading("tiltSensor", tilt, now);
    publishReading("lightSensor", lightPct, now);
    publishReading("moistureSensor", moisturePct, now);
  }
}
