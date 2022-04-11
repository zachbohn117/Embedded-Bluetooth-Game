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
#include <sstream>
#include <iostream>

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
#define SERVICE_UUID "ee88a3c5-99ab-40e0-81bb-2178ee329b9d"
#define CHARACTERISTIC_UUID "ebb77b1f-f6dd-4164-9d1b-9a00378dc46a"

// Game State variables
bool isSuccess = false;
bool gamestate = true;
bool inTask = false;
static bool isComplete = false;
static int serverPoints = 0;
static int clientPoints = 0;

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
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }
    void onDisconnect(BLEServer *pServer)
    {
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
            BLECharacteristic::PROPERTY_INDICATE);
    // bleCharacteristic->addDescriptor(new BLE2902()); // Isaiah does not have
    //  bleCharacteristic->setNotifyProperty(true);
    //  bleCharacteristic->setIndicateProperty(true);
    //  bleCharacteristic->setValue("Welcome to BOP IT");

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
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    if (deviceConnected)
    {
        M5.Lcd.fillScreen(TFT_GREEN);
        M5.Lcd.setCursor(0, 0);

        ///////////////////////////////////////////////////////////////
        // game logic to insert here
        ///////////////////////////////////////////////////////////////
        M5.update();

        int choice = rand() % 3 + 1; // picks a random value [0, 3). each value represents a game task
        if (serverPoints > 4 || clientPoints > 4)
        {
            choice = 4;
        }

        switch (choice)
        {
        case 1:
        {
            bleCharacteristic->setValue("1");
            bleCharacteristic->notify();
            // bleCharacteristic->setNotifyProperty(true);
            Serial.printf("%d written to BLE characteristic. Task to send to client\n\n\n", choice);
        }
        break;
        case 2:
        {
            bleCharacteristic->setValue("2");
            bleCharacteristic->notify();
            // bleCharacteristic->setNotifyProperty(true);
            Serial.printf("%d written to BLE characteristic. Task to send to client\n\n\n", choice);
        }
        break;
        case 3:
        {
            bleCharacteristic->setValue("3");
            bleCharacteristic->notify();
            // bleCharacteristic->setNotifyProperty(true);
            Serial.printf("%d written to BLE characteristic. Task to send to client\n\n\n", choice);
        }
        break;
        case 4:
            bleCharacteristic->setValue("3");
            bleCharacteristic->notify();
            // bleCharacteristic->setNotifyProperty(true);
            Serial.printf("%d written to BLE characteristic. Task to send to client\n\n\n", choice);
        }

        Serial.printf("current serverPoints: %i \n", serverPoints); // wjsadfjj
        Serial.printf("current clientPoints: %i \n", clientPoints); // wjsadfjj

        // if (serverPoints < 4 || clientPoints < 4)
        // {

        switch (choice)
        {
        case 1:
        { // off it
            isComplete = false;
            Serial.println("inside brighten");

            int currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
            currentAmb = currentAmb * 0.1;

            ////////////////////////////////////////////////////////////
            // UI & INSTRUCTIONS
            ////////////////////////////////////////////////////////////
            M5.Lcd.fillScreen(TFT_GREENYELLOW);
            M5.Lcd.setTextColor(BLUE);
            M5.Lcd.setTextSize(5);
            M5.Lcd.setCursor(30, sHeight / 3);
            M5.Lcd.println("LIGHT IT!");
            M5.Lcd.setTextColor(BLACK);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(30, sHeight * 2 / 3);
            M5.Lcd.println("FLASHLIGHT ON");
            M5.Lcd.setCursor(20, sHeight * 2 / 3 + 30);
            M5.Lcd.println("PROXIMITY SENSOR");
            ////////////////////////////////////////////////////////////

            while (!isComplete)
            {
                // if (currentAmb < 20)
                // { // if the brightness is too low, then set it to an artificial higher brightness
                //     currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
                //     currentAmb = currentAmb * 0.1;
                // }
                int testedAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
                testedAmb = testedAmb * 0.1;

                // Serial.printf("current ambience: %i \n", currentAmb);
                // Serial.printf("new ambience: %i \n", testedAmb);

                std::string readValueLight = bleCharacteristic->getValue();

                // Serial.printf("\nReadValue: %s \n", readValue.c_str());
                if (readValueLight == "CLIENT WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN CLIENT LOST\n");
                    clientPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }

                else if (testedAmb >= currentAmb + 30)
                {
                    bleCharacteristic->setValue("SERVER WINNER");

                    isComplete = true;
                    isSuccess = true;
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
            break;
        }
        case 2:
        { // off it
            isComplete = false;
            int currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
            currentAmb = currentAmb * 0.1;

            ////////////////////////////////////////////////////////////
            // UI & INSTRUCTIONS
            ////////////////////////////////////////////////////////////
            M5.Lcd.fillScreen(BLUE);
            M5.Lcd.setTextColor(TFT_GREENYELLOW);
            M5.Lcd.setTextSize(6);
            M5.Lcd.setCursor(30, sHeight / 3);
            M5.Lcd.println("HIDE IT!");
            M5.Lcd.setTextColor(TFT_WHITE);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(30, sHeight * 2 / 3);
            M5.Lcd.println("COVER PROXIMITY");
            M5.Lcd.setCursor(sWidth / 3, sHeight * 2 / 3 + 30);
            M5.Lcd.println("SENSOR");
            ////////////////////////////////////////////////////////////

            while (!isComplete)
            {
                if (currentAmb < 20)
                { // if the brightness is too low, then set it to an artificial higher brightness
                    currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false) + 50;
                    currentAmb = currentAmb * 0.1;
                }
                int testedAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
                testedAmb = testedAmb * 0.1;

                // Serial.printf("current ambience: %i \n", currentAmb);
                // Serial.printf("new ambience: %i \n", testedAmb);

                std::string readValueDim = bleCharacteristic->getValue();

                // Serial.printf("readValue reads: %s \n", readValue.c_str());
                if (readValueDim == "CLIENT WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN\n");
                    clientPoints++;

                    bleCharacteristic->setValue("STOP");

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }

                else if (testedAmb <= currentAmb - 20)
                {
                    bleCharacteristic->setValue("SERVER WINNER");

                    isComplete = true;
                    isSuccess = true;
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
        }
        break;
        case 3:
        { // touch it
            isComplete = false;

            ////////////////////////////////////////////////////////////
            // UI & INSTRUCTIONS
            ////////////////////////////////////////////////////////////
            M5.Lcd.fillScreen(CYAN);
            M5.Lcd.setTextColor(TFT_PURPLE);
            M5.Lcd.setCursor(35, sHeight / 3);
            M5.Lcd.setTextSize(6);
            M5.Lcd.println("BOP IT!");

            M5.Lcd.setTextColor(TFT_BLACK);
            M5.Lcd.setCursor(30, sHeight * 2 / 3);
            M5.Lcd.setTextSize(3);
            M5.Lcd.println("TAP THE SCREEN");
            ////////////////////////////////////////////////////////////
            while (!isComplete)
            {
                // Serial.printf("touch now:\n");

                std::string readValueTouch = bleCharacteristic->getValue();

                // Serial.printf("readValue reads: %s \n", readValue.c_str());
                if (readValueTouch == "CLIENT WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN\n");
                    clientPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }
                else if (M5.Touch.ispressed())
                {
                    bleCharacteristic->setValue("SERVER WINNER");

                    isComplete = true;
                    isSuccess = true;
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
            break;
        }
        case 4:
        {
            if (serverPoints > clientPoints)
            {
                M5.Lcd.fillScreen(TFT_OLIVE);
                M5.Lcd.setTextColor(TFT_LIGHTGREY);
                M5.Lcd.setTextSize(5);
                M5.Lcd.setCursor(35, 35);
                M5.Lcd.println("YOU WIN!");
            }
            else
            {
                M5.Lcd.fillScreen(TFT_MAROON);
                M5.Lcd.setTextColor(TFT_LIGHTGREY);
                M5.Lcd.setTextSize(5);
                M5.Lcd.setCursor(35, 35);
                M5.Lcd.println("YOU LOSE!");
            }

            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(35, sHeight / 2);
            M5.Lcd.printf("YOUR SCORE: %i", serverPoints);
            M5.Lcd.setCursor(35, sHeight * 3 / 4);
            M5.Lcd.printf("THER SCORE: %i", clientPoints);
            while (true)
            {
            }
        }
        break;
        }
        // }
        // else
        // {
        //     if (serverPoints > clientPoints)
        //     {
        //         M5.Lcd.fillScreen(TFT_OLIVE);
        //         M5.Lcd.setTextColor(TFT_LIGHTGREY);
        //         M5.Lcd.setTextSize(5);
        //         M5.Lcd.setCursor(35, 35);
        //         M5.Lcd.println("YOU WIN!");
        //     }
        //     else
        //     {
        //         M5.Lcd.fillScreen(TFT_MAROON);
        //         M5.Lcd.setTextColor(TFT_LIGHTGREY);
        //         M5.Lcd.setTextSize(5);
        //         M5.Lcd.setCursor(35, 35);
        //         M5.Lcd.println("YOU LOSE!");
        //     }

        //     M5.Lcd.setTextSize(3);
        //     M5.Lcd.setCursor(35, sHeight / 2);
        //     M5.Lcd.printf("YOUR SCORE: %i", serverPoints);
        //     M5.Lcd.setCursor(35, sHeight * 3 / 4);
        //     M5.Lcd.printf("THER SCORE: %i", clientPoints);

        //     delay(10000);
        // }
    }
    else
    {
        timer = 0;
        M5.Lcd.fillScreen(TFT_PURPLE);
        M5.Lcd.setTextColor(TFT_LIGHTGREY);
        M5.Lcd.setTextSize(6);
        M5.Lcd.setCursor(35, 35);
        M5.Lcd.println("BOP IT!");
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(35, sHeight / 2);
        M5.Lcd.printf("WAITING FOR PLAYER");
    }

    // Only update the timer (if connected) every 1 second
    delay(1000);
}