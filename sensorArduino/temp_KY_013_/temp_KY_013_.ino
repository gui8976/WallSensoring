const int LM35_PIN = A0;
const int NUM_SAMPLES = 32;

void setup() {
  Serial.begin(9600);
  analogReadResolution(12);
}

void loop() {
  // Average several readings to reduce ADC noise
  long sumMv = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sumMv += analogReadMilliVolts(LM35_PIN);
    delay(2);
  }
  float mV = sumMv / (float)NUM_SAMPLES;

  float tempC = mV / 10.0;   // LM35: 10 mV per °C

  Serial.print("Voltage: ");
  Serial.print(mV);
  Serial.print(" mV\t Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");

  delay(1000);
}