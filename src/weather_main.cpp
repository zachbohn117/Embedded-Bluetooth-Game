#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////
// TODO 3: Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "5f6aefe3a8c1ddca7cb7e48c2e23d4d9";

// TODO 1: WiFi variables
String wifiNetworkName = "CBU";
String wifiPassword = "";

// Time limit variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)
signed long btnAClicks = 0;
signed long btnCClicks = 0;

// Set up time components
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Actual time variables
String formattedDate;

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
String
httpGETRequest(const char *serverName);
String zipString();
void drawWeatherImage(String iconId, int resizeMult);
void drawMainScreen();
void drawZipcodeScreen();
void drawZipcode(int32_t x, int i);

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Initialize the device
    M5.begin();

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
    timeClient.setTimeOffset(-28800);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    M5.update();

    // Changes between Farenheit and Celsius
    if (M5.BtnA.wasPressed())
    {
        btnAClicks++;

        drawMainScreen();
    }

    // Changes between Weather and ZIP screens
    if (M5.BtnC.wasPressed())
    {
        btnCClicks++;

        if (btnCClicks % 2 == 0)
        {
            drawMainScreen();
        }
        else
        {
            drawZipcodeScreen();
        }
    }

    // Refreshes Weather OR ZIP screen
    if (btnCClicks % 2 == 0)
    {
        if ((millis() - lastTime) > timerDelay)
        {
            drawMainScreen();

            lastTime = millis(); // Update the last time to NOW
        }
    }
    else
    {
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

String zipString()
{
    String string = "";

    for (int i = 0; i < 5; i++)
    {
        string += zipCode[i];
    }
    return string;
}

void drawMainScreen()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        timeClient.update();
        formattedDate = timeClient.getFormattedTime();
        int hours = (timeClient.getHours() < 13) ? timeClient.getHours() : timeClient.getHours() - 12;
        int min = timeClient.getMinutes();
        int seconds = timeClient.getSeconds();

        //////////////////////////////////////////////////////////////////
        // TODO 4: Hardcode the specific city,state,country into the query
        // Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
        //////////////////////////////////////////////////////////////////
        // api.openweathermap.org/data/2.5/weather?zip={zip code},{country code}&appid={API key}
        // String serverURL = urlOpenWeather + "q=Kleinfeltersville,pa,usa&units=imperial&appid=" + apiKey;
        String zipCodeString = zipString();
        String serverURL = urlOpenWeather + "zip=" + zipCodeString + "&units=imperial" + "&appid=" + apiKey;

        // Serial.println(serverURL); // Debug print

        //////////////////////////////////////////////////////////////////
        // TODO 5: Make GET request and store reponse
        //////////////////////////////////////////////////////////////////
        String response = httpGETRequest(serverURL.c_str());
        // Serial.print(response); // Debug print

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
        DeserializationError error = deserializeJson(objResponse, response);
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
        double tempNowF = objMain["temp"];
        double tempMinF = objMain["temp_min"];
        double tempMaxF = objMain["temp_max"];
        Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMax: %.1f F\n", tempNowF, strWeatherDesc, tempMinF, tempMaxF);

        // Getting Celcius  C = (F - 32 ) * 5/9
        double tempNowC = (tempNowF - 32) * 5 / 9;
        double tempMinC = (tempMinF - 32) * 5 / 9;
        double tempMaxC = (tempMaxF - 32) * 5 / 9;
        Serial.printf("NOW: %.1f C and %s\tMIN: %.1f C\tMax: %.1f C\n", tempNowC, strWeatherDesc, tempMinC, tempMaxC);

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
        // TODO 12: Draw background - light blue if day time and navy blue of night
        //////////////////////////////////////////////////////////////////
        uint16_t primaryTextColor;
        if (strWeatherIcon.indexOf("d") >= 0)
        {
            M5.Lcd.fillScreen(TFT_CYAN);
            primaryTextColor = TFT_DARKGREY;
        }
        else
        {
            M5.Lcd.fillScreen(TFT_NAVY);
            primaryTextColor = TFT_WHITE;
        }

        //////////////////////////////////////////////////////////////////
        // TODO 13: Draw the icon on the right side of the screen - the built in
        // drawBitmap method works, but we cannot scale up the image
        // size well, so we'll call our own method
        //////////////////////////////////////////////////////////////////
        // M5.Lcd.drawBitmap(0, 0, 100, 100, myBitmap, TFT_BLACK);
        drawWeatherImage(strWeatherIcon, 3);

        //////////////////////////////////////////////////////////////////
        // This code will draw the temperature centered on the screen; left
        // it out in favor of another formatting style
        //////////////////////////////////////////////////////////////////
        /*
        M5.Lcd.setTextColor(TFT_WHITE);
        textSize = 10;
        M5.Lcd.setTextSize(textSize);
        int iTempNowF = tempNowF;
        tWidth = textSize * String(iTempNowF).length() * 5;
        tHeight = textSize * 5;
        M5.Lcd.setCursor(sWidth/2 - tWidth/2,sHeight/2 - tHeight/2);
        M5.Lcd.print(iTempNowF);
        */

        //////////////////////////////////////////////////////////////////
        // TODO 14: Draw the temperatures and city name
        //////////////////////////////////////////////////////////////////
        int pad = 10;
        M5.Lcd.setCursor(pad, pad);
        M5.Lcd.setTextColor(TFT_BLUE);
        M5.Lcd.setTextSize(3);
        if (btnAClicks % 2 == 0)
        {
            M5.Lcd.printf("LO:%0.fF\n", tempMinF);
        }
        else
        {
            M5.Lcd.printf("LO:%0.fC\n", tempMinC);
        }

        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.setTextSize(10);
        if (btnAClicks % 2 == 0)
        {
            M5.Lcd.printf("LO:%0.fF\n", tempNowF);
        }
        else
        {
            M5.Lcd.printf("LO:%0.fC\n", tempNowC);
        }

        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setTextSize(3);
        if (btnAClicks % 2 == 0)
        {
            M5.Lcd.printf("LO:%0.fF\n", tempMaxF);
        }
        else
        {
            M5.Lcd.printf("LO:%0.fC\n", tempMaxC);
        }

        int cityTextSize = (cityName.length() < 15) ? 3 : 2;
        M5.Lcd.setTextSize(cityTextSize);
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.printf("%s\n", cityName.c_str());

        M5.Lcd.setTextSize(3);
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.printf("%d:%d:%d", hours, min, seconds);

        // setting zipButtons

        // degrees button switch
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(pad + 5, M5.Lcd.getCursorY() + 80);
        M5.Lcd.setTextColor(primaryTextColor);
        if (btnAClicks % 2 == 0)
        {
            M5.Lcd.printf("F/C");
        }
        else
        {
            M5.Lcd.printf("C/F");
        }
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(220, M5.Lcd.getCursorY());
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.printf("Zip Code");
    }
    else
    {
        Serial.println("WiFi Disconnected");
    }
}
void drawZipcodeScreen()
{
    // draw zip code gui
    M5.Lcd.fillScreen(BLACK);

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
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(sWidth - (xSpace * 4), sHeight - 20);
        M5.Lcd.printf("Enter");
    }

    class Point
    {
    public:
        int32_t x;
        int32_t y;
    };
}

void drawZipcode(int32_t x, int i)
{
    int y = 85;
    int offset = 10;

    M5.Lcd.setCursor(x - offset, y);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(5);

    M5.Lcd.print(zipCode[i]);
}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following links:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
// https://docs.m5stack.com/en/api/core2/lcd_api
//////////////////////////////////////////////////////////////////////////////////