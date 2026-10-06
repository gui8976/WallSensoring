#include <DHT.h>

#define DHTPIN 2      // Data pin connected to the Nano
#define DHTTYPE DHT11 // KY-015 uses the DHT11 sensor

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  Serial.println("KY-015 (DHT11) Test");
  dht.begin();
}

void loop() {
  delay(2000); // DHT11 can only be read about once per second, 2s is safe

  float h = dht.readHumidity();
  float t = dht.readTemperature();        // Celsius
  float f = dht.readTemperature(true);    // Fahrenheit, optional

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false); // heat index in Celsius

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" °C\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.println(" °C");
}