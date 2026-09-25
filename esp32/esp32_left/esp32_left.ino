#include <WiFi.h>
#include <PubSubClient.h>

// ==============================================================================
// CONFIGURATION
// ==============================================================================

// Wi-Fi Credentials
const char* WIFI_SSID     = "SYSTEC-FoF_Collab";
const char* WIFI_PASSWORD = "systec_collaborative";

// MQTT Broker Settings (PC/Pi IP running broker.py)
const char* MQTT_SERVER   = "10.227.18.51";
const int   MQTT_PORT     = 1883;

// MQTT Credentials
const char* MQTT_USER     = "DIGI";
const char* MQTT_PASS     = "PHD";

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
// SENSOR CALIBRATION / CONVERSION CONSTANTS
// ==============================================================================
const float ADC_MAX  = 4095.0;   // 12-bit ADC resolution (matches analogReadResolution(12))
const float ADC_VREF = 3.3;      // ESP32 ADC reference voltage

// SEN0114 moisture sensor
const int MOISTURE_AIR_VALUE   = 3000; // raw ADC reading in dry air  -> 0%
const int MOISTURE_WATER_VALUE = 1200; // raw ADC reading in water    -> 100%

// DFR0026 light sensor - DFRobot gives no official lux formula 
const float LIGHT_GAMMA = 0.7;
const float LIGHT_RL10  = 50.0;

// ==============================================================================
// CONVERSION HELPERS
// ==============================================================================

// DFR0026 raw ADC -> approximate lux
float rawToLux(int rawADC) {
  float voltage = rawADC / ADC_MAX * ADC_VREF;
  if (voltage <= 0.0) voltage = 0.001;               // avoid divide-by-zero
  if (voltage >= ADC_VREF) voltage = ADC_VREF - 0.001;
  float resistance = 2000.0 * voltage / (ADC_VREF - voltage);
  float lux = pow(LIGHT_RL10 * 1e3 * pow(10, LIGHT_GAMMA) / resistance, (1.0 / LIGHT_GAMMA));
  return lux;
}

// SEN0114 raw ADC -> soil moisture percentage (needs the calibration above)
float rawToMoisturePercent(int rawADC) {
  float pct = map(rawADC, MOISTURE_AIR_VALUE, MOISTURE_WATER_VALUE, 0, 100);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return pct;
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
  while (!Serial) {
    ; // Wait for the serial port to connect
  }
  Serial.println("Hello, world!");
  Serial.println("[System]");
  analogReadResolution(12); // 0-4095, matches MicroPython's default ADC range

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

    // Step A: Read raw values from this board's 3 sensors
    int tiltRaw     = digitalRead(PIN_TILT);      // DFR0028: 0 or 1, already meaningful
    int lightRaw    = analogRead(PIN_LIGHT);      // DFR0026: 0-4095
    int moistureRaw = analogRead(PIN_MOISTURE);   // SEN0114: 0-4095

    // Step B: Convert to real-world units
    float lightLux    = rawToLux(lightRaw);
    float moisturePct = rawToMoisturePercent(moistureRaw);

    // Step C: Publish each reading as its own message (same timestamp for the cycle)
    publishReading("tiltSensor", tiltRaw, now);
    publishReading("lightSensor", lightLux, now);
    publishReading("moistureSensor", moisturePct, now);
  }
}
