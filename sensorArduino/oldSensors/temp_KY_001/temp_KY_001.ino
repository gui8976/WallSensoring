// Required libraries will be imported
#include <OneWire.h>
#include <DallasTemperature.h>            

// Here the input pin is declared to which the sensor module is connected
#define KY001_Signal_PIN 4

// Libraries are configured
OneWire oneWire(KY001_Signal_PIN);          
DallasTemperature sensors(&oneWire);    


void setup() {

    // Initialize serial output
    Serial.begin(9600);
    Serial.println("KY-001 temperature measurement");

    // Sensor is initialized
    sensors.begin();  
}

//main program loop
void loop()
{
    // Temperature measurement is started...
    sensors.requestTemperatures();
    // ... and measured temperature is printed.
    Serial.print("Temperature: ");
    Serial.print(sensors.getTempCByIndex(0));
    Serial.println(" °C");

    delay(1000); // 1s pause until next measurement
}
