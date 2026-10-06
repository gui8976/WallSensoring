const int LIGHT_PIN = A0;
const int NUM_SAMPLES = 16;


const int DARK_MV   = 100;    
const int BRIGHT_MV = 3000;   

void setup() {
  Serial.begin(9600);
  analogReadResolution(12);
}

void loop() {
  long sumMv = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sumMv += analogReadMilliVolts(LIGHT_PIN);
    delay(2);
  }
  int mV = sumMv / NUM_SAMPLES;

  int lightPercent = map(mV, DARK_MV, BRIGHT_MV, 0, 100);
  lightPercent = constrain(lightPercent, 0, 100);

  Serial.print("Voltage: ");
  Serial.print(mV);
  Serial.print(" mV\t Light: ");
  Serial.print(lightPercent);
  Serial.println(" %");

  delay(500);
}