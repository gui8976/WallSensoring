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
// SENSOR SETUP
// ==============================================================================
// NOTE: These were raw classic-ESP32 GPIO numbers in the MicroPython version.
// The Arduino Nano ESP32 (ESP32-S3) has a different pinout - check the board's
// pinout diagram and swap these for the correct pins (often labeled A0/A1/A2)
// if readings look wrong.
const int PIN_TEMPERATURE   = 34;
const int PIN_HUMIDITY      = 35;
const int PIN_EXTENSOMETER  = 32;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 2000; // 2 seconds

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

  connectWiFi();
  connectMQTT();

  Serial.println("[System] Starting continuous sensor monitoring loop...");
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

    // Step A: Read live values from all 3 sensors
    int val1 = 1;//analogRead(PIN_TEMPERATURE);
    int val2 = 2;//analogRead(PIN_HUMIDITY);
    int val3 = 3;//analogRead(PIN_EXTENSOMETER);

    // Step B + C: Build JSON payload directly (no ArduinoJson dependency needed
    // for a payload this simple)
    char payload[200];
    snprintf(payload, sizeof(payload),
      "{\"device_id\":\"%s\",\"temperatureSensor_val\":%d,\"humiditySensor_val\":%d,\"extensometerSensor_val\":%d,\"timestamp_ms\":%lu}",
      CLIENT_ID, val1, val2, val3, now);

    // Step D: Publish to topic
    Serial.print("[Publishing] Topic: ");
    Serial.print(MQTT_TOPIC);
    Serial.print(" | Payload: ");
    Serial.println(payload);

    client.publish(MQTT_TOPIC, payload);
  }
}
