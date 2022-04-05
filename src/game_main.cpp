#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"
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

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////

// Time variables
static unsigned long gameTimer;
static unsigned long taskTimer;
static unsigned long secondsOn = 0;
bool isSuccess = false;

// I2C Pins
const int PIN_SDA = 32;
const int PIN_SCL = 33;

// Sensor variables
float tempC = 0;
float humd = 0;
int ambience;

// LCD variables
int sWidth;
int sHeight;

// shake variables
float accX;
float accY;
float accZ;

// enum COLORS {GREEN, RED, BLUE, CYAN, PURPLE};
// TODO: Timer for each task and instruction screen (me)
// TODO: Bluetooth connection (me?)
// TODO: Points? (zach)
// TODO: Winner screen/Loser screen (zach)

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
bool coolOrHeat(bool isHeat, int currentTemp, int currentHum);
bool dim(int currentAmb);
bool turnOff();
bool touch();
bool shake();
void instuctionScreen(int task);
void successScreen(bool isSuccessful);

void setup()
{
    M5.begin();
    I2C_RW::initI2C(0x44, 400000, 32, 33, true);

    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL, false);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);

    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    gameTimer = millis();
    taskTimer = millis();
    M5.Lcd.fillScreen(PURPLE);
    M5.Lcd.println("BOP IT!");
}

void loop()
{
    M5.update();

    // M5.Axp.SetLcdVoltage(2800);

    int choice = 2; // picks a random value [0, 5). each value represents a game task
    int currentTemp = tempC;
    int currentHumd = humd;

    I2C_RW::getShtTempHum(&tempC, &humd);
    secondsOn++;
    if (millis() >= gameTimer + 5000)
    {
        Serial.printf("millis(): %u \n", millis());
        Serial.printf("gameTimer: %u \n", gameTimer);
        isSuccess = false;
        Serial.printf("Choice!:  %u!\n", choice);
        gameTimer = millis();
        Serial.printf("Hello World! (%u secs)!\n", secondsOn++);
        switch (choice)
        {
        case 0:
        {
            instuctionScreen(choice);
            Serial.printf("get instructions!\n");
            isSuccess = coolOrHeat(true, currentTemp, currentHumd);
            if (isSuccess)
            {
                // successScreen();
            } // heat it
        }
        break;
        case 1:
        {
            Serial.printf("get instructions!\n");
            // coolOrHeat(false, currentTemp, currentHumd);    // cool it
        }
        break;
        case 2:
        {
            if (millis() >= taskTimer + 5000)
            {

                // Serial.printf("print instructions!\n");
                // M5.Lcd.fillScreen(BLUE);
                // M5.Lcd.println("DIM IT!");
                // Serial.printf("get instructions!\n");
                int currentAmb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
                currentAmb = currentAmb * 0.1;
                Serial.printf("starter ambience: %u!\n", currentAmb);

                taskTimer = millis();
                isSuccess = dim(currentAmb);
                if (isSuccess)
                {
                    // successScreen();
                } // dim it
            }
        }
        break;
        case 3:
        {
            instuctionScreen(choice);
            Serial.printf("get instructions!\n");
            bool isSuccess = turnOff();
            if (isSuccess)
            {
                // successScreen();
            } // off it
        }
        break;
        case 4:
        {
            instuctionScreen(choice);
            Serial.printf("get instructions!\n");
            bool isSuccess = touch();
            if (isSuccess)
            {
                // successScreen();
            } // touch it
        }
        break;
        case 5:
        {
            instuctionScreen(choice);
            Serial.printf("get instructions!\n");
            bool isSuccess = shake();
            if (isSuccess)
            {
                // successScreen();
            } // shake it
        }
        break;
        default:
        {
            instuctionScreen(choice);
            Serial.printf("default!\n");
            bool isSuccess = shake();
        }
        }
        // if (millis() >= gameTimer + 100) {
        // successScreen(isSuccess);
        // }
    }
}

////////////////////////////////////////////////////////////////////
// Game Tasks
////////////////////////////////////////////////////////////////////

bool coolOrHeat(bool isHeat, int currentTemp, int currentHum)
{
    int temp = tempC;
    int humid = humd;
    I2C_RW::getShtTempHum(&tempC, &humd);

    if (isHeat)
    { // heating task
        if (currentHum < humid)
        {
            return true; // got heated, good
        }
    }
    else
    { // cooling task
        if (currentHum > humid)
        {
            return true; // got cooled, good
        }
    }
}
bool dim(int currentAmb)
{
    Serial.printf("inside dim method\n");
    Serial.printf("millis(): %u \n", millis());
    Serial.printf("taskTimer: %u \n\n", taskTimer);
    // if (millis() >= taskTimer + 5000) {
    Serial.printf("print instructions!\n");
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.println("DIM IT!");
    Serial.printf("get instructions!\n\n");

    taskTimer = millis();
    // Dimming and brightening screen based on light
    int amb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
    amb = amb * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux
    // Serial.printf("Ambient Light: %d\n", amb);

    // Min screen value = 2500
    // Max screen value = 2800
    // 150 val for amb will give max brightness
    int brightness = (150 < amb) ? 150 : amb;
    M5.Axp.SetLcdVoltage(2500 + (brightness * 2));

    int offset = currentAmb - 50;
    Serial.printf("current ambience: %i \n", currentAmb);
    Serial.printf("new ambience: %i \n", amb);
    if (brightness < offset)
    {
        Serial.println("success");
        return true; // dimmed screen by at least 50 units, good
    }
    // }
}
bool turnOff()
{
    int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
    // Serial.printf("Proximity: %d\n", prox);
    if (prox < 75)
    {
        M5.Lcd.writecommand(ILI9341_DISPON);
        return true; // completed turning screen off, good
    }
    else
    {
        M5.Lcd.writecommand(ILI9341_DISPOFF);
    }
}
bool touch()
{
    if (M5.Touch.ispressed())
    {
        TouchPoint tp = M5.Touch.getPressPoint();
        Serial.printf("\tYou pressed touch screen @ (%d, %d)\n", tp.x, tp.y);
        return true; // screen touched, good
    }
}
bool shake()
{
    Serial.printf("shake please!\n");
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    Serial.printf("X=%.2f\n", accX);
    Serial.printf("Y=%.2f\n", accY);
    Serial.printf("Z=%.2f\n", accZ);
    accX = abs(accX) * 9.8;
    accY = abs(accY) * 9.8;
    accZ = abs(accZ) * 9.8;

    Serial.printf("X=%.2f\n\n", accX);
    Serial.printf("Y=%.2f\n\n", accY);
    Serial.printf("Z=%.2f\n\n", accZ);
    if (accX > 10 && accY > 10 && accZ > 10)
    {
        Serial.print("We are here\n");
        return true; // shook, good
    }
    else
    {
        return false;
    }
}

////////////////////////////////////////////////////////////////////
// Screens
////////////////////////////////////////////////////////////////////

void instuctionScreen(int task)
{
    M5.Lcd.setTextColor(BLACK);
    switch (task)
    {
    case 0:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(PURPLE);
        M5.Lcd.println("HEAT IT");
        break;
    case 1:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(RED);
        M5.Lcd.println("HEAT IT");
        break;
    case 2:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(BLUE);
        M5.Lcd.println("DIM IT!");
        break;
    case 3:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(CYAN);
        M5.Lcd.println("OFF IT!");
        break;
    case 4:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(PURPLE);
        M5.Lcd.println("TOUCH IT!");
        break;
    case 5:
        Serial.printf("print instructions!\n");
        M5.Lcd.fillScreen(GREEN);
        M5.Lcd.println("SHAKE IT!");
        break;
    }
}
void successScreen(bool isSuccessful)
{
    if (isSuccessful)
    {
        M5.Lcd.setTextColor(BLACK);
        M5.Lcd.fillScreen(GREEN);
        M5.Lcd.println("CORRECT!");
    }
}
