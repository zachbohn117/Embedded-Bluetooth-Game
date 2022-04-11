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
#include "../include/I2C_RW.h"

///////////////////////////////////////////////////////////////
// Register definitions
///////////////////////////////////////////////////////////////
#define VCNL_I2C_ADDRESS 0x60
#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;
bool deviceConnected = false;
int timer = 0;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "f9d6e404-4062-4919-bfc7-494dbbb64ac3"
#define CHARACTERISTIC_UUID "ebb77b1f-f6dd-4164-9d1b-9a00378dc46a"

// Game State variables
bool isSuccess = false;
bool gamestate = true;
bool inTask = false;
static int points = 0;

// I2C Pins
const int PIN_SDA = 32;
const int PIN_SCL = 33;

// Sensor variables
float tempC = 0;
float humd = 0;
float comparingHumd = 0;
int ambience;

// LCD variables
int sWidth;
int sHeight;

// shake variables
float accX;
float accY;
float accZ;

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
    // M5.Lcd.fillScreen(TFT_CYAN);
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
    // Serial.println("characteristic defined...you can connect with your phone!");
    // M5.Lcd.fillScreen(TFT_BLUE);

    ///////////////////////////////////////////////////////////////
    // game set up here
    ///////////////////////////////////////////////////////////////

    I2C_RW::initI2C(0x44, 400000, 32, 33, true);

    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL, false);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);

    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    M5.Lcd.fillScreen(PURPLE);
    M5.Lcd.println("BOP IT!");
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


        ///////////////////////////////////////////////////////////////
        // game logic to insert here
        ///////////////////////////////////////////////////////////////
        M5.update();

        int choice = rand() % 3; // picks a random value [0, 3). each value represents a game task
        
        Serial.printf("current points: %i \n", points);

        if (points < 3) {

            switch (choice) {
                case 0: {  // cool it
                    bool isComplete = false;

                    int preHumd = humd;

                    I2C_RW::getShtTempHum(&tempC, &humd);
                    
                    while (!isComplete) {  // TODO: completes too soon. see to fix
                        if (preHumd > 40) {
                            preHumd = 40;
                        }
                        int testedHumd = comparingHumd;
                        
                        I2C_RW::getShtTempHum(&tempC, &comparingHumd);
                        M5.Lcd.fillScreen(RED);
                        M5.Lcd.println("COOL IT!");

                        Serial.printf("current humdity: %i \n", preHumd);
                        Serial.printf("new humidity: %i \n", testedHumd);
                        
                        if (preHumd < testedHumd) {
                            points++;
                            isComplete = true;
                            isSuccess = true;
                            for (int i = 0; i < 100; i++) { // creates a delay after completing a task
                                Serial.printf("tick # %i \n", i);
                                M5.Lcd.fillScreen(GREEN);
                            }
                        }
                    }
                }
                break;
                case 1: { // off it
                    bool isComplete = false;
                    int currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
                    currentAmb = currentAmb * 0.1;
                    
                    while (!isComplete) {
                        if (currentAmb < 20) { // if the brightness is too low, then set it to an artificial higher brightness
                            currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
                            currentAmb = currentAmb * 0.1;
                        }
                        int testedAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
                        testedAmb = testedAmb * 0.1;

                        M5.Lcd.fillScreen(BLUE);
                        M5.Lcd.println("DIM IT!");

                        Serial.printf("current ambience: %i \n", currentAmb);
                        Serial.printf("new ambience: %i \n", testedAmb);
                        
                        if (testedAmb <= currentAmb - 20) {
                            isComplete = true;
                            isSuccess = true;
                            points++;

                            for (int i = 0; i < 100; i++) { // creates a delay after completing a task
                                Serial.printf("tick # %i \n", i);
                                M5.Lcd.fillScreen(GREEN);
                            }
                        }
                    }
                }
                break;
                case 2: {  // touch it
                    bool isComplete = false;
                    
                    while (!isComplete) {

                        M5.Lcd.fillScreen(CYAN);
                        M5.Lcd.println("TOUCH IT!");

                        Serial.printf("touch now:\n");

                        if (M5.Touch.ispressed()) {
                            isComplete = true;
                            isSuccess = true;
                            TouchPoint tp = M5.Touch.getPressPoint();
                            Serial.printf("\tYou pressed touch screen @ (%d, %d)\n", tp.x, tp.y);
                            points++;
                            
                            for (int i = 0; i < 100; i++) { // creates a delay after completing a task
                                Serial.printf("tick # %i \n", i);
                                M5.Lcd.fillScreen(GREEN);
                            }
                        } 
                    }
                }
                break;
                inTask = false;
            } 
        } else {
            M5.Lcd.fillScreen(OLIVE);
        }



    } else {
        timer = 0;
        M5.Lcd.fillScreen(TFT_RED);
        M5.Lcd.setCursor(0,0);
        M5.Lcd.println("Disconnected");
    }
    
    // Only update the timer (if connected) every 1 second
    delay(1000);
}