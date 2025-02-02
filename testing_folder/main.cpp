#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
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

// LCD variables 320x240
int sWidth;
int sHeight;

// shake variables
float accX;
float accY;
float accZ;

// TODO: Timer for each task and instruction screen (me)
// TODO: Bluetooth connection (me?)
// TODO: Points? (zach)
// TODO: Winner screen/Loser screen (zach)

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

    M5.Lcd.fillScreen(TFT_PURPLE);
    M5.Lcd.println("BOP IT!");
}

void loop()
{
    M5.update();

    int choice = rand() % 3; // picks a random value [0, 3). each value represents a game task

    Serial.printf("current points: %i \n", points);

    if (points < 3)
    {

        switch (choice)
        {
        ////////////////////////////////////////////////////////////
        // COOL IT :: Blow on temp sensor
        ////////////////////////////////////////////////////////////
        case 0:
        { // cool it
            bool isComplete = false;

            int preHumd = humd;

            I2C_RW::getShtTempHum(&tempC, &humd);

            ////////////////////////////////////////////////////////////
            // UI & INSTRUCTIONS
            ////////////////////////////////////////////////////////////
            M5.Lcd.fillScreen(RED);
            M5.Lcd.setTextColor(TFT_CYAN);
            M5.Lcd.setTextSize(6);
            M5.Lcd.setCursor(30, sHeight / 3);
            M5.Lcd.println("COOL IT!");
            M5.Lcd.setTextColor(TFT_WHITE);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setCursor(30, sHeight * 2 / 3);
            M5.Lcd.println("BLOW ON TEMP");
            M5.Lcd.setCursor(sWidth / 3, sHeight * 2 / 3 + 30);
            M5.Lcd.println("SENSOR");
            ////////////////////////////////////////////////////////////

            while (!isComplete)
            { // TODO: completes too soon. see to fix
                if (preHumd > 40)
                {
                    preHumd = 40;
                }
                int testedHumd = comparingHumd;

                I2C_RW::getShtTempHum(&tempC, &comparingHumd);

                Serial.printf("current humdity: %i \n", preHumd);
                Serial.printf("new humidity: %i \n", testedHumd);

                if (preHumd < testedHumd)
                {
                    points++;
                    isComplete = true;
                    isSuccess = true;
                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        Serial.printf("tick # %i \n", i);
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
        }
        break;
        ////////////////////////////////////////////////////////////
        // HIDE IT :: Put hand on proximity sensor
        ////////////////////////////////////////////////////////////
        case 1:
        { // dim it
            bool isComplete = false;
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
            M5.Lcd.setCursor(sWidth/3, sHeight * 2 / 3+30);
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

                Serial.printf("current ambience: %i \n", currentAmb);
                Serial.printf("new ambience: %i \n", testedAmb);

                if (testedAmb <= currentAmb - 20)
                {
                    isComplete = true;
                    isSuccess = true;
                    points++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        Serial.printf("tick # %i \n", i);
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
        }
        break;
        ////////////////////////////////////////////////////////////
        // BOP IT :: Tap the screen
        ////////////////////////////////////////////////////////////
        case 2:
        { // touch it
            bool isComplete = false;

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

                Serial.printf("touch now:\n");
                if (M5.Touch.ispressed())
                {
                    isComplete = true;
                    isSuccess = true;
                    TouchPoint tp = M5.Touch.getPressPoint();
                    Serial.printf("\tYou pressed touch screen @ (%d, %d)\n", tp.x, tp.y);
                    points++;

                    for (int i = 0; i < 100; i++)
                    { // creates a delay after completing a task
                        Serial.printf("tick # %i \n", i);
                        M5.Lcd.fillScreen(GREEN);
                    }
                }
            }
        }
        break;
            inTask = false;
        }
    }
    else
    {
        bool win = false;
        int opPoints = 5;

        if (win){
            M5.Lcd.fillScreen(TFT_OLIVE);
            M5.Lcd.setTextColor(TFT_LIGHTGREY);
            M5.Lcd.setTextSize(5);
            M5.Lcd.setCursor(35, 35);
            M5.Lcd.println("YOU WIN!");
        }
        else{
            M5.Lcd.fillScreen(TFT_MAROON);
            M5.Lcd.setTextColor(TFT_LIGHTGREY);
            M5.Lcd.setTextSize(5);
            M5.Lcd.setCursor(35, 35);
            M5.Lcd.println("YOU LOSE!");
        }

        M5.Lcd.setTextColor(TFT_LIGHTGREY);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setCursor(35, sHeight / 2);
        M5.Lcd.printf("YOUR SCORE: %i", points);
        M5.Lcd.setCursor(35, sHeight * 3/4);
        M5.Lcd.printf("THER SCORE: %i", opPoints);
    }
}