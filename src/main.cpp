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
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_BME680.h>
#include <ArduinoJson.h>

//#define TEST
#define RELEASE
//#define RELEASE_LOG

#define KEY_PAIR_NUM 8
const size_t CAPACITY = JSON_OBJECT_SIZE(KEY_PAIR_NUM);
StaticJsonDocument<CAPACITY> doc;
JsonObject object = doc.to<JsonObject>();
String payload;

// Initialize BLE
String serverPrefix = "Clip-";
String serverId;
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define ms_TO_S_FACTOR 1000 // Conversion factor for mili seconds to seconds
#define TIME_INTERVAL 5     // Time interval ESP32 will send data and repeat (in seconds)

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
#if defined(TEST) || defined(RELEASE_LOG)
    Serial.println("[INFO] deviceConnected");
#endif
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
#if defined(TEST) || defined(RELEASE_LOG)
    Serial.println("[INFO] deviceDisconnected");
#endif
  }
};

// Initialize Moisture Sensor
#define MOISTURE_ADC_PIN 34
const int AirValue = 0;   // TODO - max 4095 (12 bits ADC)
const int WaterValue = 0; // TODO
int wetValue = 0;

// Initialize LIS3DH
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Initialize BME680
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme;

void setup(void)
{
#if defined(TEST) || defined(RELEASE_LOG)
  Serial.begin(9600);
  while (!Serial)
    delay(10); // will pause until serial console opens
#endif

#if defined(TEST) || defined(RELEASE_LOG)
  Serial.println("LIS3DH & BME680 starting...!");
#endif

  // LIS3DH supports 0x18 (default) or 0x19 address
  // BME680 supports 0x77 (default) or 0x76 address
  if (!lis.begin(0x18) || !bme.begin(0x77))
  {
#if defined(TEST) || defined(RELEASE_LOG)
    Serial.println("Couldn't start...");
#endif
    while (1)
      yield();
  }
#if defined(TEST) || defined(RELEASE_LOG)
  Serial.println("LIS3DH & BME680 found!");
#endif

  // Set up LIS3DH (accelerometer range)
  lis.setRange(LIS3DH_RANGE_4_G); // 2, 4, 8 or 16 G!

  // Set up BME680 (oversampling and filter initialization)
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  // Set up BLE
  serverId = serverPrefix + WiFi.macAddress()[9] +
             WiFi.macAddress()[10] +
             WiFi.macAddress()[12] +
             WiFi.macAddress()[13] +
             WiFi.macAddress()[15] +
             WiFi.macAddress()[16];
  BLEDevice::init(serverId.c_str());

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
#if defined(TEST) || defined(RELEASE_LOG)
  Serial.println("[INFO] waiting a client connection to notify...");
#endif
}

void loop()
{
#if defined(TEST)
  // MOISTURE
  wetValue = analogRead(MOISTURE_ADC_PIN);
  wetPercent = map(moistureValue, AirValue, WaterValue, 0, 100);
  Serial.print("Wet = ");
  if (wetPercent >= 100)
  {
    Serial.println("100 %");
  }
  else if (wetPercent <= 0)
  {
    Serial.println("0 %");
  }
  else if (wetPercent > 0 && wetPercent < 100)
  {
    Serial.print(wetPercent);
    Serial.println("%");
  }

  // LIS3DH displays values (acceleration is measured in m/s^2)
  sensors_event_t event;
  lis.getEvent(&event);
  Serial.print("Axis = ");
  Serial.print(" X: ");
  Serial.print(event.acceleration.x);
  Serial.print(" Y: ");
  Serial.print(event.acceleration.y);
  Serial.print(" Z: ");
  Serial.print(event.acceleration.z);
  Serial.println(" m/s^2 ");

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
  delay(TIME_INTERVAL * ms_TO_S_FACTOR);
#else

  // notify changed value
  if (deviceConnected)
  {

#ifdef RELEASE
    // Execute sensors and wrap data in json payload
    // Device ID
    object["id"] = serverId.c_str();

    // MOISTURE
    wetValue = analogRead(MOISTURE_ADC_PIN);
    object["wet"] = wetValue;

    // LIS3DH
    sensors_event_t event;
    lis.getEvent(&event);
    object["ax"] = event.acceleration.x;
    object["ay"] = event.acceleration.y;
    object["az"] = event.acceleration.z;

    // BME680
    if (!bme.performReading())
    {
#ifdef RELEASE_LOG
      Serial.println("Failed to perform reading :(");
#endif
      return;
    }
    object["temp"] = bme.temperature;
    object["gas"] = bme.gas_resistance / 1000.0;
    //object["pres"] = bme.pressure / 100.0;
    //object["alt"] = bme.readAltitude(SEALEVELPRESSURE_HPA);
    //object["hum"] = bme.humidity;

    serializeJson(object, payload); // create a minified JSON document
#ifdef RELEASE_LOG
    Serial.println(payload);
    Serial.print("Length: ");
    Serial.println(payload.length());
#endif

    pCharacteristic->setValue(payload.c_str()); // convert JSON to char and send
    pCharacteristic->notify();
#ifdef RELEASE_LOG
    Serial.println("[INFO] sent data");
#endif
    payload = "";
#endif

    delay(TIME_INTERVAL * ms_TO_S_FACTOR);
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
#ifdef RELEASE_LOG
    Serial.println("[INFO] start advertising");
#endif
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
#endif
}