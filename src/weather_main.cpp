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

// I2C Pins
const int PIN_SDA = 32;
const int PIN_SCL = 33;

// TODO 3: Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "5f6aefe3a8c1ddca7cb7e48c2e23d4d9";

// TODO 1: WiFi variables
String wifiNetworkName = "CBU-LancerArmsEast";
String wifiPassword = "";

// Time limit variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)

// Button Variables
bool unitIsCelsius = false;
bool btnBWasClicked = false;
char screenState = 'A';

// Set up time components
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Weather screen variables
String apiResponse;
int hours;
int minutes;
int seconds;

// Sensor variables
float tempC = 0;
float humd = 0;

// LCD variables
int sWidth;
int sHeight;

// ZIP Code
int zipCode[5] = {9, 2, 5, 0, 4}; // default Riverside, CA
Button zipButtons[10] = {Button(0, 0, 50, 50), Button(65, 0, 50, 50), Button(130, 0, 50, 50), Button(195, 0, 50, 50), Button(260, 0, 50, 50),
                         Button(0, 150, 50, 50), Button(65, 150, 50, 50), Button(130, 150, 50, 50), Button(195, 150, 50, 50), Button(260, 150, 50, 50)};

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
String httpGETRequest(const char *serverName);
void drawWeatherImage(String iconId, int resizeMult);
void makeTimeUpdate();
void makeApiRequest();
void drawWeatherScreen();
void drawSensorsScreen();
void drawZipcodeScreen();
void drawButtonLabels(uint16_t textColor);

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Initialize the device
    M5.begin();
    I2C_RW::initI2C(0x44, 400000, 32, 33, true);

    // Connect to VCNL sensor
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL, false);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
    I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);

    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // TODO 2: Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    // set up time zone
    timeClient.begin();
    timeClient.setTimeOffset(-25200);

    // initial API call
    makeApiRequest();
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    M5.update();

    //////////////////////////////////////////////////////////////////////////////////
    // Page Button Handling
    //////////////////////////////////////////////////////////////////////////////////
    // Changes between Farenheit and Celsius
    if (M5.BtnA.wasPressed())
    {
        unitIsCelsius = !unitIsCelsius;
        if (screenState == 'A')
            drawWeatherScreen();
        if (screenState == 'C')
            drawSensorsScreen();
    }
    //////////////////////////////////////////////////////////////////////////////////
    // Changes between Weather and Temp/Humidity Screens
    if (M5.BtnB.wasPressed())
    {
        if (screenState == 'B')
        {
            screenState = 'A';
            drawWeatherScreen();
        }
        else
        {
            screenState = 'B';
            btnBWasClicked = true;
            drawSensorsScreen();
        }
    }
    //////////////////////////////////////////////////////////////////////////////////
    // Changes between Weather and ZIP screens
    if (M5.BtnC.wasPressed())
    {
        if (screenState == 'C')
        {
            screenState = 'A';
            makeTimeUpdate();
            makeApiRequest();
            drawWeatherScreen();
        }
        else
        {
            screenState = 'C';
            drawZipcodeScreen();
        }
    }

    //////////////////////////////////////////////////////////////////////////////////
    // Page Screen Handling
    //////////////////////////////////////////////////////////////////////////////////
    // Draw Weather Screen A
    if (screenState == 'A')
    {
        if ((millis() - lastTime) > timerDelay)
        {
            makeTimeUpdate();
            makeApiRequest();
            drawWeatherScreen();
            lastTime = millis(); // Update the last time to NOW
        }
    }
    //////////////////////////////////////////////////////////////////////////////////
    // Draw Temp/Humidity Screen B
    if (screenState == 'B')
    {
        if ((millis() - lastTime) > timerDelay)
        {
            drawSensorsScreen();
            lastTime = millis();
        }
    }
    //////////////////////////////////////////////////////////////////////////////////
    // Draw ZIP Code Screen C
    if (screenState == 'C')
    {
        // Adjust zipcode for button presses
        for (int i = 0; i < 10; i++)
        {
            if (zipButtons[i].wasPressed())
            {
                if (i < 5)
                {
                    zipCode[i] = (zipCode[i] + 1) % 10;
                }
                else
                {
                    zipCode[i % 5] = zipCode[i % 5] - 1;
                    if (zipCode[i % 5] < 0)
                    {
                        zipCode[i % 5] = 10 - 1;
                    }
                }
                drawZipcodeScreen();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////
    // I2C proximity portion
    //////////////////////////////////////////////////////////////////////////////////
    // I2C calls to read sensor data

    // Turning screen on and off based on proximity
    int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
    // Serial.printf("Proximity: %d\n", prox);
    if (prox < 100)
    {
        M5.Lcd.writecommand(ILI9341_DISPON);
    }
    else
    {
        M5.Lcd.writecommand(ILI9341_DISPOFF);
    }

    // Dimming and brightening screen based on light
    int amb = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
    amb = amb * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux
    // Serial.printf("Ambient Light: %d\n", amb);

    // Min screen value = 2500
    // Max screen value = 2800
    // 150 val for amb will give max brightness
    int brightness = (150 < amb) ? 150 : amb;
    M5.Axp.SetLcdVoltage(2500 + (brightness * 2));
}

/////////////////////////////////////////////////////////////////
// This method takes in a URL and makes a GET request to the
// URL, returning the response.
/////////////////////////////////////////////////////////////////
String httpGETRequest(const char *serverURL)
{

    // Initialize client
    HTTPClient http;
    http.begin(serverURL);

    // Send HTTP GET request and obtain response
    int httpResponseCode = http.GET();
    String response = http.getString();

    // Check if got an error
    if (httpResponseCode > 0)
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    else
    {
        Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
        Serial.printf("Server Response: %s\n", response);
    }

    // Free resources and return response
    http.end();
    return response;
}

/////////////////////////////////////////////////////////////////
// This method takes in an image icon string (from API) and a
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically).
/////////////////////////////////////////////////////////////////
void drawWeatherImage(String iconId, int resizeMult)
{

    // Get the corresponding byte array
    const uint16_t *weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    int xOffset = sWidth - (imgSqDim * resizeMult * .8); // Right align (image doesn't take up entire array)
    // int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally

    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++)
    {
        for (int x = 0; x < imgSqDim; x++)
        {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = weatherBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0)
            {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++)
                {
                    for (int j = 0; j < resizeMult; j++)
                    {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}

void makeTimeUpdate(){
    if (WiFi.status() == WL_CONNECTED) {
        //////////////////////////////////////////////////////////////////
        // Call TimeClient: Get Date and Time of API Call
        //////////////////////////////////////////////////////////////////
        timeClient.update();
        hours = (timeClient.getHours() < 13) ? timeClient.getHours() : timeClient.getHours() - 12;
        minutes = timeClient.getMinutes();
        seconds = timeClient.getSeconds();
    }
    else
    {
        Serial.println("WiFi Disconnected");
    }
}

void makeApiRequest()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        //////////////////////////////////////////////////////////////////
        // TODO 4: Hardcode the specific city,state,country into the query
        // Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
        //////////////////////////////////////////////////////////////////
        // api.openweathermap.org/data/2.5/weather?zip={zip code},{country code}&appid={API key}
        // String serverURL = urlOpenWeather + "q=Kleinfeltersville,pa,usa&units=imperial&appid=" + apiKey;
        String zipCodeString = "";
        for (int i = 0; i < 5; i++)
        {
            zipCodeString += zipCode[i];
        }
        String serverURL = urlOpenWeather + "zip=" + zipCodeString + "&units=imperial" + "&appid=" + apiKey;
        // Serial.println(serverURL); // Debug print

        //////////////////////////////////////////////////////////////////
        // TODO 5: Make GET request and store reponse
        //////////////////////////////////////////////////////////////////
        apiResponse = httpGETRequest(serverURL.c_str());
        // Serial.print(response); // Debug print
    }
    else
    {
        Serial.println("WiFi Disconnected");
    }
}

void drawWeatherScreen()
{
    //////////////////////////////////////////////////////////////////
    // TODO 6: Import ArduinoJSON Library and then use arduinojson.org/v6/assistant to
    // compute the proper capacity (this is a weird library thing) and initialize
    // the json object
    //////////////////////////////////////////////////////////////////
    const size_t jsonCapacity = 768 + 250;
    DynamicJsonDocument objResponse(jsonCapacity);

    //////////////////////////////////////////////////////////////////
    // TODO 7: (uncomment) Deserialize the JSON document and test if parsing succeeded
    //////////////////////////////////////////////////////////////////
    DeserializationError error = deserializeJson(objResponse, apiResponse);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    // serializeJsonPretty(objResponse, Serial); // Debug print

    //////////////////////////////////////////////////////////////////
    // TODO 8: Parse Response to get the weather description and icon
    //////////////////////////////////////////////////////////////////
    JsonArray arrWeather = objResponse["weather"];
    JsonObject objWeather0 = arrWeather.getElement(0);
    String strWeatherDesc = objWeather0["main"];
    String strWeatherIcon = objWeather0["icon"];
    String cityName = objResponse["name"];

    // TODO 9: Parse response to get the temperatures
    JsonObject objMain = objResponse["main"];
    float tempNowF = objMain["temp"];
    float tempMinF = objMain["temp_min"];
    float tempMaxF = objMain["temp_max"];
    Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMAX: %.1f F\n", tempNowF, strWeatherDesc, tempMinF, tempMaxF);

    // Getting Celcius  C = (F - 32 ) * 5/9
    float tempNowC = (tempNowF - 32) * 5 / 9;
    float tempMinC = (tempMinF - 32) * 5 / 9;
    float tempMaxC = (tempMaxF - 32) * 5 / 9;
    Serial.printf("NOW: %.1f C and %s\tMIN: %.1f C\tMAX: %.1f C\n", tempNowC, strWeatherDesc, tempMinC, tempMaxC);

    //////////////////////////////////////////////////////////////////
    // TODO 10: We can download the image directly, but there is no easy
    // way to work with a PNG file on the ESP32 (in Arduino) so we will
    // take another route - see EGR425_Phase1_weather_bitmap_images.h
    // for more details
    //////////////////////////////////////////////////////////////////
    // String imagePath = "http://openweathermap.org/img/wn/" + strWeatherIcon + "@2x.png";
    // Serial.println(imagePath);
    // response = httpGETRequest(imagePath.c_str());
    // Serial.print(response);

    //////////////////////////////////////////////////////////////////
    // TODO 12: Draw background - light blue if day time and dark blue of night
    //////////////////////////////////////////////////////////////////
    uint16_t primaryTextColor;
    if (strWeatherIcon.indexOf("d") >= 0)
    {
        M5.Lcd.fillScreen(0x077F);
        primaryTextColor = TFT_BLACK;
    }
    else
    {
        M5.Lcd.fillScreen(0x0007);
        primaryTextColor = TFT_LIGHTGREY;
    }

    //////////////////////////////////////////////////////////////////
    // TODO 13: Draw the icon on the right side of the screen - the built in
    // drawBitmap method works, but we cannot scale up the image
    // size well, so we'll call our own method
    //////////////////////////////////////////////////////////////////
    // M5.Lcd.drawBitmap(0, 0, 100, 100, myBitmap, TFT_BLACK);
    drawWeatherImage(strWeatherIcon, 3);

    //////////////////////////////////////////////////////////////////
    // TODO 14: Draw the temperatures and city name
    //////////////////////////////////////////////////////////////////
    int pad = 10;

    // Print Forecast High
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(3);
    unitIsCelsius ? M5.Lcd.printf("HI:%0.fC\n", tempMaxC) : M5.Lcd.printf("HI:%0.fF\n", tempMaxF);

    // Print Current Tempurature
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(10);
    unitIsCelsius ? M5.Lcd.printf("%0.fC\n", tempNowC) : M5.Lcd.printf("%0.fF\n", tempNowF);

    // Print Forecast Low
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setTextSize(3);
    unitIsCelsius ? M5.Lcd.printf("LO:%0.fC\n", tempMinC) : M5.Lcd.printf("LO:%0.fF\n", tempMinF);

    // Print City Name
    int cityTextSize = (cityName.length() < 15) ? 3 : 2;
    M5.Lcd.setTextSize(cityTextSize);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_DARKGREEN);
    M5.Lcd.printf("%s\n", cityName.c_str());

    // Print Time of API call
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.printf("%d:%d:%d", hours, minutes, seconds);

    drawButtonLabels(primaryTextColor);
}

void drawSensorsScreen()
{
    int preTemp = tempC;
    int preHumd = humd;

    I2C_RW::getShtTempHum(&tempC, &humd);

    if ((preTemp != (int)tempC || preHumd != (int)humd) || btnBWasClicked)
    {
        btnBWasClicked = false;

        Serial.printf("temp: %f\nhum: %f\n", tempC, humd);

        M5.Lcd.fillScreen(TFT_BLACK);

        int pad = 10;
        M5.Lcd.setCursor(pad, pad);
        M5.Lcd.setTextColor(0xF540);
        M5.Lcd.setTextSize(3);
        M5.Lcd.printf("Local Weather");

        M5.Lcd.fillCircle(245, 15, 5, TFT_RED);
        M5.Lcd.setCursor(255, M5.Lcd.getCursorY());
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.println("LIVE");

        M5.Lcd.setCursor(pad * 2, sHeight * 1 / 4);
        M5.Lcd.setTextColor(TFT_CYAN);
        M5.Lcd.setTextSize(3);
        unitIsCelsius ? M5.Lcd.printf("Temp: %0.fC", tempC)
                      : M5.Lcd.printf("Temp: %0.fF", (tempC * 9.0 / 5.0) + 32.0);

        M5.Lcd.setCursor(pad * 2, sHeight / 2);
        M5.Lcd.setTextColor(TFT_GREENYELLOW);
        M5.Lcd.printf("Humidity: %0.f%%", humd);

        makeTimeUpdate();
        M5.Lcd.setCursor(pad * 2, sHeight * 3 / 4);
        M5.Lcd.setTextColor(TFT_LIGHTGREY);
        M5.Lcd.printf("%d:%d:%d", hours, minutes, seconds);
        
        drawButtonLabels(TFT_LIGHTGREY);
    }
    else
    {
        Serial.println("No Temp or Humd change");
    }
}

void drawZipcodeScreen()
{
    // draw zip code gui
    M5.Lcd.fillScreen(TFT_BLACK);

    int start = 25;
    int xSpace = 15;
    int ySpace = 150;

    for (int i = 0; i < 5; i++)
    {
        int space = ((start * 2) + xSpace) * i; // space between each triagnle

        // up arrows
        Point top = Point(start + space, start);
        Point botLeft = Point(start * 2 + space, start * 2);
        Point botRight = Point(0 + space, start * 2);

        M5.Lcd.fillTriangle(top.x, top.y, botRight.x, botRight.y, botLeft.x, botLeft.y, TFT_DARKCYAN);

        // down arrows
        Point bot = Point(start + space, start + ySpace);
        Point topLeft = Point(0 + space, 0 + ySpace);
        Point topRight = Point(start * 2 + space, 0 + ySpace);

        M5.Lcd.fillTriangle(bot.x, bot.y, topLeft.x, topLeft.y, topRight.x, topRight.y, TFT_DARKCYAN);

        // draw zip code digits
        int y = 85;
        int offset = 10;

        M5.Lcd.setCursor(top.x - offset, y);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setTextSize(5);
        M5.Lcd.print(zipCode[i]);
    }

    drawButtonLabels(TFT_LIGHTGREY);

    class Point
    {
    public:
        int32_t x;
        int32_t y;
    };
}

//////////////////////////////////////////////////////////////////
// TODO 15: Draw Button Labels
//////////////////////////////////////////////////////////////////
void drawButtonLabels(uint16_t textColor)
{
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(textColor);

    // Convert Temp Units Button
    M5.Lcd.setCursor(32, 220);
    unitIsCelsius ? M5.Lcd.printf("C->F") : M5.Lcd.printf("F->C");

    // Show Sensor Temp and Humidity
    M5.Lcd.setCursor(sWidth / 2 - 40, M5.Lcd.getCursorY());
    M5.Lcd.printf("Sensors");

    // Change ZIP Code Button
    M5.Lcd.setCursor(250, M5.Lcd.getCursorY());
    M5.Lcd.printf("ZIP");
}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following links:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
// https://docs.m5stack.com/en/api/core2/lcd_api
//////////////////////////////////////////////////////////////////////////////////
