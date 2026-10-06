int ThermistorPin = A2;
int Vo;

float R1 = 10000; // fixed resistor value on the KY-013 board (10k)

// Steinhart-Hart coefficients for this thermistor
float c1 = 0.001129148;
float c2 = 0.000234125;
float c3 = 0.0000000876741;

float logR2, R2, T;

void setup() {
  Serial.begin(9600);
}

void loop() {
  Vo = analogRead(ThermistorPin);

  R2 = R1 * (float)Vo / (1023.0 - Vo); // flipped divider formula
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
  T = T - 273.15;

  Serial.print("Raw ADC: ");
  Serial.print(Vo);
  Serial.print("\t Resistance: ");
  Serial.print(R2);
  Serial.print("\t Temperature: ");
  Serial.print(T);
  Serial.println(" °C");

  delay(1000);
}