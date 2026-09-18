// SEN0114 - DFRobot Gravity Analog Soil Moisture Sensor
// Wiring: VCC -> 5V, GND -> GND, Signal (A0/AO) -> A0

const int SENSOR_PIN = A0;

// Typical calibration values (adjust after testing your own sensor):
// ~ dry air / out of soil  -> near 1023 (or a bit lower, e.g. 900-1023)
// ~ fully submerged in water -> around 300-500
const int AIR_VALUE = 1023;
const int WATER_VALUE = 400;

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  int rawValue = analogRead(SENSOR_PIN);

  // Map raw reading to a 0-100% moisture scale
  int moisturePercent = map(rawValue, AIR_VALUE, WATER_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print("  |  Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  delay(1000);
}