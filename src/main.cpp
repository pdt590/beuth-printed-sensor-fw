/*
** References
** - https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
** - https://learn.adafruit.com/adafruit-lis3dh-triple-axis-accelerometer-breakout?view=all
** - https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas?view=all
**
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_BME680.h>

// Initialize LIS3DH
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Initialize BME680
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme;

void setup(void)
{
  Serial.begin(9600);
  while (!Serial)
    delay(10); // will pause until serial console opens

  Serial.println("LIS3DH & BME680 starting...!");

  // LIS3DH supports 0x18 (default) or 0x19 address
  // BME680 supports 0x77 (default) or 0x76 address
  if (!lis.begin(0x18) || !bme.begin(0x77))
  {
    Serial.println("Couldn't start...");
    while (1)
      yield();
  }
  Serial.println("LIS3DH & BME680 found!");

  // Set up LIS3DH (accelerometer range)
  lis.setRange(LIS3DH_RANGE_4_G); // 2, 4, 8 or 16 G!

  // Set up BME680 (oversampling and filter initialization)
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop()
{
  // LIS3DH displays values (acceleration is measured in m/s^2)
  sensors_event_t event;
  lis.getEvent(&event);
  Serial.print("\t\tX: ");
  Serial.print(event.acceleration.x);
  Serial.print(" \tY: ");
  Serial.print(event.acceleration.y);
  Serial.print(" \tZ: ");
  Serial.print(event.acceleration.z);
  Serial.println(" m/s^2 ");

  Serial.println();

  // BME680 displays values
  if (!bme.performReading())
  {
    Serial.println("Failed to perform reading :(");
    return;
  }
  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println();

  // delay
  delay(2000);
}