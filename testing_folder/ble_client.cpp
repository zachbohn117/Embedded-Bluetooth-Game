// Some useful resources on BLE and ESP32:
//      https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/examples/BLE_notify/BLE_notify.ino
//      https://microcontrollerslab.com/esp32-bluetooth-low-energy-ble-using-arduino-ide/
//      https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
//      https://www.electronicshub.org/esp32-ble-tutorial/
#include <BLEDevice.h>
//#include <BLEServer.h>
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
static boolean doConnect = false;
// static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;
bool deviceConnected = false;
int timer = 0;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
static BLEUUID SERVICE_UUID("ee88a3c5-99ab-40e0-81bb-2178ee329b9d");        // Dr. Dan's Service
static BLEUUID CHARACTERISTIC_UUID("ebb77b1f-f6dd-4164-9d1b-9a00378dc46a"); // Zach's Characteristic

// Game State variables
bool isSuccess = false;
bool gamestate = true;
bool inTask = false;
int clientPoints = 0;
int serverPoints = 0;
bool isComplete = false;

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
// BLE Client Callback Methods
///////////////////////////////////////////////////////////////
static void notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char *)pData);
}

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

bool connectToServer()
{
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient *pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr)
    {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(SERVICE_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(CHARACTERISTIC_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
        // std::string value = pRemoteCharacteristic->readValue();
        // Serial.print("The characteristic value was: ");
        // Serial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

    deviceConnected = true;
    return deviceConnected;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());
        Serial.printf("\tAddress: %s\n", advertisedDevice.getAddress().toString().c_str());
        Serial.printf("\tHas a ServiceUUID: %s\n", advertisedDevice.haveServiceUUID() ? "True" : "False");
        for (int i = 0; i < advertisedDevice.getServiceUUIDCount(); i++)
        {
            Serial.printf("\t\t%s\n", advertisedDevice.getServiceUUID(i).toString().c_str());
        }
        Serial.printf("\tHas our service: %s\n\n", advertisedDevice.isAdvertisingService(SERVICE_UUID) ? "True" : "False");
        // Serial.println(advertisedDevice.toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(SERVICE_UUID))
        {

            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;

        } // Found our server
    }     // onResult
};        // M

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Init device
    M5.begin();
    M5.Lcd.setTextSize(3);
    // Serial.print("Starting BLE...");

    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL, false);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);

    M5.Lcd.fillScreen(PURPLE);
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setCursor(35, sHeight / 3);
    M5.Lcd.setTextSize(6);
    M5.Lcd.println("BOP IT!");

    M5.Lcd.setTextColor(TFT_LIGHTGREY);
    M5.Lcd.setCursor(30, sHeight * 2 / 3);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Waiting for server");
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (doConnect == true)
    {
        if (connectToServer())
        {
            Serial.println("We are now connected to the BLE Server.");
        }
        else
        {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
        }
        doConnect = false; // sadlfkjsldj
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    // here is where the client side logic is supposed to be at.... i think
    if (deviceConnected)
    {
        Serial.println("Connected to other M5");

        ///////////////////////////////////////////////////////////////
        // game logic to insert here
        ///////////////////////////////////////////////////////////////
        M5.update();

        Serial.printf("current serverPoints: %i \n", serverPoints); // wjsadfjj
        Serial.printf("current clientPoints: %i \n", clientPoints); // wjsadfjj

        std::string cvalue = pRemoteCharacteristic->readValue();
        String choiceValue = cvalue.c_str();
        Serial.println(choiceValue.c_str());

        int choiceTask = choiceValue.toInt();

        if (serverPoints > 4 || clientPoints > 4)
        {
            choiceTask = 4;
        }

        switch (choiceTask)
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
            M5.Lcd.setTextSize(6);
            M5.Lcd.setCursor(30, sHeight / 3);
            M5.Lcd.println("LIGHT IT!");
            M5.Lcd.setTextColor(BLACK);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(30, sHeight * 2 / 3);
            M5.Lcd.println("FLASHLIGHT ON");
            M5.Lcd.setCursor(30, sHeight * 2 / 3 + 30);
            M5.Lcd.println("PROXIMITY SENSOR");
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

                std::string readValue = pRemoteCharacteristic->readValue();

                // Serial.printf("\nReadValue: %s \n", readValue.c_str());
                if (readValue == "SERVER WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN CLIENT LOST\n");
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }

                else if (testedAmb >= currentAmb + 30)
                {
                    pRemoteCharacteristic->writeValue("CLIENT WINNER");

                    isComplete = true;
                    isSuccess = true;
                    clientPoints++;

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
            Serial.println("inside dim");

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

                std::string readValue = pRemoteCharacteristic->readValue();

                // Serial.printf("\nReadValue: %s \n", readValue.c_str());
                if (readValue == "SERVER WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN CLIENT LOST\n");
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }

                else if (testedAmb <= currentAmb - 20)
                {
                    pRemoteCharacteristic->writeValue("CLIENT WINNER");

                    isComplete = true;
                    isSuccess = true;
                    clientPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
            break;
        }
        case 3:
        { // touch it
            isComplete = false;
            Serial.println("inside touch");

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

                std::string readValue = pRemoteCharacteristic->readValue();

                // Serial.printf("\nReadValue: %s \n", readValue.c_str());
                if (readValue == "SERVER WINNER")
                {
                    isComplete = true;
                    Serial.printf("CHANGE TO LOST TASK SCREEN CLIENT LOST\n");
                    serverPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(MAROON);
                    }
                }

                else if (M5.Touch.ispressed())
                {
                    pRemoteCharacteristic->writeValue("CLIENT WINNER");

                    isComplete = true;
                    isSuccess = true;
                    clientPoints++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
            break;
        case 4:
        {
            if (clientPoints > serverPoints)
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

            M5.Lcd.setTextColor(TFT_LIGHTGREY);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(35, sHeight / 2);
            M5.Lcd.printf("YOUR SCORE: %i", clientPoints);
            M5.Lcd.setCursor(35, sHeight * 3 / 4);
            M5.Lcd.printf("THER SCORE: %i", serverPoints);
        }
        break;
        }
        }
    }
    else if (doScan)
    {
        BLEDevice::getScan()->start(0); // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
    }

    delay(1000); // Delay a second between loops.
}