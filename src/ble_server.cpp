// Some useful resources on BLE and ESP32:
//      https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/examples/BLE_notify/BLE_notify.ino
//      https://microcontrollerslab.com/esp32-bluetooth-low-energy-ble-using-arduino-ide/
//      https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
//      https://www.electronicshub.org/esp32-ble-tutorial/
#include <BLEDevice.h>
#include <BLEServer.h>
// #include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>


///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;
bool deviceConnected = false;
int timer = 0;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Device connected...");
    }
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Init device
    M5.begin();
    M5.Lcd.setTextSize(3);
    Serial.print("Starting BLE...");

    // Initialize M5Core2 as a BLE server
    BLEDevice::init("My M5Core2");
    M5.Lcd.fillScreen(TFT_CYAN);
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());
    bleService = bleServer->createService(SERVICE_UUID);
    bleCharacteristic = bleService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    //bleCharacteristic->addDescriptor(new BLE2902()); // Isaiah does not have
    // bleCharacteristic->setNotifyProperty(true);
    // bleCharacteristic->setIndicateProperty(true);
    bleCharacteristic->setValue("Hello BLE World from Dr. Dan!");

    bleService->start();

    // Start broadcasting (advertising) BLE service
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true); // Isaiah has true
    // bleAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connection issues
    bleAdvertising->setMinPreferred(0x12); // Isaiah has
    // bleAdvertising->setMinPreferred(0x00); // Set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    Serial.println("characteristic defined...you can connect with your phone!");
    M5.Lcd.fillScreen(TFT_BLUE);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    if (deviceConnected) {
        // 1 - Update the characteristic's value (which can  be read by a client)
        // timer++;
        // bleCharacteristic->setValue(timer);
        // bleCharacteristic->notify();
        // //bleCharacteristic->setNotifyProperty(true);
        //Serial.printf("%d written to BLE characteristic.\n", timer);

        // 2 - Read the characteristic's value as a string (which can be written from a client)
        std::string readValue = bleCharacteristic->getValue();
        Serial.printf("The new characteristic value as a STRING is: %s\n", readValue.c_str());
        String valStr = readValue.c_str();
        int val = valStr.toInt();
        Serial.printf("The new characteristic value as an INT is: %d\n", val);

        // 3 - Read the characteristic's value as a BYTE (which can be written from a client)
        // uint8_t *numPtr = new uint8_t();
        // numPtr = bleCharacteristic->getData();
        // Serial.printf("The new characteristic value as a BYTE is: %d\n", *numPtr);

        M5.Lcd.fillScreen(TFT_GREEN);
        M5.Lcd.setCursor(0,0);
        M5.Lcd.println(readValue.c_str());
    } else {
        timer = 0;
        M5.Lcd.fillScreen(TFT_RED);
        M5.Lcd.setCursor(0,0);
        M5.Lcd.println("Disconnected");
    }
    
    // Only update the timer (if connected) every 1 second
    delay(1000);
}