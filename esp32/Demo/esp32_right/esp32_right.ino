#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

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
const char* CLIENT_ID     = "ESP32_right";  // Must be unique per ESP32

// ==============================================================================
// SENSOR SETUP — this board carries 3 physical sensors:
//   1) SEN0114 - Analog Soil Moisture Sensor
//   2) KY-015  - DHT11 Temperature + Humidity module (digital, single-wire)
//   3) LM35    - DFRobot LM35 Temperature Sensor V4 (analog, 10 mV/°C)
// Pin names are the Nano ESP32's own labels (D2, A0, A1) - A0/A1 sit on
// ADC1, which stays reliable while Wi-Fi is active.
// ==============================================================================
const int PIN_MOISTURE = A0;  // SEN0114 - analog pin (ADC1)
const int PIN_DHT      = D2;  // KY-015  - digital data pin
const int PIN_LM35     = A1;  // LM35    - analog pin (ADC1)

#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

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

// LM35 temperature sensor
const int   LM35_SAMPLES   = 32;   // readings averaged per measurement
const float LM35_OFFSET_C  = 0.0;  // correction vs. a reference thermometer

// ==============================================================================
// SENSOR READING FUNCTIONS
// ==============================================================================

// SEN0114: raw ADC -> 0-100 % (dry = low, wet = high)
int readMoisturePercent() {
  int rawValue = analogRead(PIN_MOISTURE);
  int percent = map(rawValue, DRY_VALUE, WET_VALUE, 0, 100);
  return constrain(percent, 0, 100);
}

// LM35: averaged millivolts -> °C (10 mV per °C)
float readLM35Celsius() {
  long sumMv = 0;
  for (int i = 0; i < LM35_SAMPLES; i++) {
    sumMv += analogReadMilliVolts(PIN_LM35);
    delay(2);
  }
  float mV = sumMv / (float)LM35_SAMPLES;
  return mV / 10.0 + LM35_OFFSET_C;
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
  pinMode(PIN_MOISTURE, INPUT);
  dht.begin();

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
    int   moisturePct = readMoisturePercent();   // 0-100 %
    float dhtHumidity = dht.readHumidity();      // %RH
    float dhtTemp     = dht.readTemperature();   // °C
    float lm35C       = readLM35Celsius();       // °C

    // Step B: Publish each reading as its own message (same timestamp for the cycle)
    publishReading("moistureSensor_pct", moisturePct, now);

    // DHT11 reads can occasionally fail (returns NaN)
    if (!isnan(dhtHumidity)) {
      publishReading("dhtHumidity_pct", dhtHumidity, now);
    } else {
      Serial.println("[Warn] DHT11 humidity read failed, skipping.");
    }
    if (!isnan(dhtTemp)) {
      publishReading("dhtTemperature_c", dhtTemp, now);
    } else {
      Serial.println("[Warn] DHT11 temperature read failed, skipping.");
    }

    // Key name kept as in the old sketch so the broker/database/Grafana keep working
    publishReading("thermistorTemperature_c", lm35C, now);
  }
}
