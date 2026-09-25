#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>

// ==============================================================================
// CONFIGURATION
// ==============================================================================

// Wi-Fi Credentials
const char* WIFI_SSID     = "SYSTEC-FoF_Collab";
const char* WIFI_PASSWORD = "XXXXX";

// MQTT Broker Settings (PC/Pi IP running broker.py)
const char* MQTT_SERVER   = "10.227.18.51";
const int   MQTT_PORT     = 1883;

// Topics & Identifiers
const char* MQTT_TOPIC    = "esp32/sensorReadings";
const char* CLIENT_ID     = "ESP32_right";  // Must be unique per ESP32

// ==============================================================================
// SENSOR SETUP — this board carries 3 physical sensors:
//   1) SEN0114 - Analog Soil Moisture Sensor
//   2) KY-015  - DHT11 Temperature + Humidity module (digital, single-wire)
//   3) KY-013  - NTC Thermistor Temperature module (analog)
// Pin names are the Nano ESP32's own labels (D2, A0, A1) - A0/A1 sit on
// ADC1, which stays reliable while Wi-Fi is active.
// ==============================================================================
const int PIN_MOISTURE   = A0;  // SEN0114 - analog pin (ADC1)
const int PIN_DHT        = D2;  // KY-015 - digital data pin
const int PIN_THERMISTOR = A1;  // KY-013 - analog pin (ADC1)

#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 5000; // 5 seconds

// ==============================================================================
// SENSOR CALIBRATION / CONVERSION CONSTANTS
// ==============================================================================
const float ADC_MAX  = 4095.0;   // 12-bit ADC resolution
const float ADC_VREF = 3.3;      // ESP32 ADC reference voltage

// SEN0114 moisture sensor - calibrate for YOUR sensor: dip in dry air, note
// the raw analogRead() value, then dip in a cup of water and note that one.
const int MOISTURE_AIR_VALUE   = 3000; // raw ADC reading in dry air  -> 0%
const int MOISTURE_WATER_VALUE = 1200; // raw ADC reading in water    -> 100%

// KY-013 thermistor - matches the module's onboard 10k series resistor and
// standard Steinhart-Hart coefficients (same as DFRobot/Joy-IT reference code)
const float THERMISTOR_R1 = 10000.0;
const float SH_C1 = 0.001129148;
const float SH_C2 = 0.000234125;
const float SH_C3 = 0.0000000876741;

// ==============================================================================
// CONVERSION HELPERS
// ==============================================================================

// SEN0114 raw ADC -> soil moisture percentage (needs the calibration above)
float rawToMoisturePercent(int rawADC) {
  float pct = map(rawADC, MOISTURE_AIR_VALUE, MOISTURE_WATER_VALUE, 0, 100);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// KY-013 raw ADC -> temperature in Celsius via Steinhart-Hart equation
float rawToThermistorCelsius(int rawADC) {
  if (rawADC <= 0) rawADC = 1; // avoid divide-by-zero
  float r2 = THERMISTOR_R1 * ((ADC_MAX / (float)rawADC) - 1.0);
  float logR2 = log(r2);
  float tempK = 1.0 / (SH_C1 + SH_C2 * logR2 + SH_C3 * logR2 * logR2 * logR2);
  return tempK - 273.15;
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
  analogReadResolution(12); // 0-4095

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

    // Step A: Read raw / sensor-native values
    int moistureRaw = analogRead(PIN_MOISTURE);       // SEN0114: 0-4095
    float dhtHumidity = dht.readHumidity();           // KY-015: %RH
    float dhtTemp = dht.readTemperature();             // KY-015: °C
    int thermistorRaw = analogRead(PIN_THERMISTOR);   // KY-013: 0-4095

    // Step B: Convert to real-world units
    float moisturePct = rawToMoisturePercent(moistureRaw);
    float thermistorC = rawToThermistorCelsius(thermistorRaw);

    // Step C: Publish each reading as its own message (same timestamp for the cycle)
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

    publishReading("thermistorTemperature_c", thermistorC, now);
  }
}
