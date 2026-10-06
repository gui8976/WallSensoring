

const int SENSOR_PIN = A0;


const int DRY_VALUE = 0;       // sensor in air
const int WET_VALUE = 3000;    // sensor in water

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);
  analogReadResolution(12);    // 0-4095 on ESP32
}

void loop() {
  int rawValue = analogRead(SENSOR_PIN);

  // Dry (low) -> 0%, wet (high) -> 100%
  int moisturePercent = map(rawValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print("  |  Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  delay(1000);
}