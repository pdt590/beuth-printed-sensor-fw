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
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_BME680.h>
#include <ArduinoJson.h>

//#define DEBUG
//#define DEV
#define JSON

#define KEY_PAIR_NUM 8
const size_t CAPACITY = JSON_OBJECT_SIZE(KEY_PAIR_NUM);
StaticJsonDocument<CAPACITY> doc;
JsonObject object = doc.to<JsonObject>();
String payload;

// Initialize BLE
String serverName;
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define ms_TO_S_FACTOR 1000 // Conversion factor for mili seconds to seconds
#define TIME_INTERVAL 3     // Time interval ESP32 will send data and repeat (in seconds)

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
#if defined(DEBUG) || defined(DEV)
    Serial.println("[INFO] deviceConnected");
#endif
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
#if defined(DEBUG) || defined(DEV)
    Serial.println("[INFO] deviceDisconnected");
#endif
  }
};

// Initialize LIS3DH
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Initialize BME680
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme;

void setup(void)
{
#if defined(DEBUG) || defined(DEV)
  Serial.begin(9600);
  while (!Serial)
    delay(10); // will pause until serial console opens
#endif

#if defined(DEBUG) || defined(DEV)
  Serial.println("LIS3DH & BME680 starting...!");
#endif

  // LIS3DH supports 0x18 (default) or 0x19 address
  // BME680 supports 0x77 (default) or 0x76 address
  if (!lis.begin(0x18) || !bme.begin(0x77))
  {
#if defined(DEBUG) || defined(DEV)
    Serial.println("Couldn't start...");
#endif
    while (1)
      yield();
  }
#if defined(DEBUG) || defined(DEV)
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
  // Create the BLE Device
  serverName = "ESP32-01";
  //serverName += String(random(0xffff), HEX);
  BLEDevice::init(serverName.c_str());

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
#if defined(DEBUG) || defined(DEV)
  Serial.println("[INFO] waiting a client connection to notify...");
#endif
}

void loop()
{
#if defined(DEBUG)
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
#else

  // notify changed value
  if (deviceConnected)
  {

#ifdef JSON
    // Execute sensors and wrap data in json payload
    // Device Name
    object["name"] = serverName;

    // LIS3DH
    sensors_event_t event;
    lis.getEvent(&event);
    object["ax"] = event.acceleration.x;
    object["ay"] = event.acceleration.y;
    object["az"] = event.acceleration.z;

    // BME680
    if (!bme.performReading())
    {
#ifdef DEV
      Serial.println("Failed to perform reading :(");
#endif
      return;
    }
    object["temp"] = bme.temperature;
    object["pres"] = bme.pressure / 100.0;
    object["hum"] = bme.humidity;
    object["gas"] = bme.gas_resistance / 1000.0;
    object["alt"] = bme.readAltitude(SEALEVELPRESSURE_HPA);
    serializeJson(object, payload); // create a minified JSON document
#ifdef DEV
    Serial.println(payload);
    Serial.print("Length: ");
    Serial.println(payload.length());
#endif

    pCharacteristic->setValue(payload.c_str()); // convert JSON to char and send
    pCharacteristic->notify();
#ifdef DEV
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
#ifdef DEV
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