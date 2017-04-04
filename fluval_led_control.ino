/**************************
 * Fluval LED Control
 * for ESP8266-12E (NodeMCU) modules
 * (C)2017 by Steve Luzynski except where noted 
 * 
 * Wiring: 
 * 
 * IR Control: Connect an IR LED to GPIO4 through a 330 Ohm resistor to ground.
 * 
 * Water Temp: Connect a waterproof DS18B20 to 3v3 & ground. Connect signal to GPIO2
 * and pull signal up through a 120 Ohm resistor.
 * 
 * Inside temp: Coonect a TMP36 to A0, 3v3, and ground.
 * 
 * Codes in buttons.h reversed by pointing the OEM remote at an
 * ESP based IR decoder module, no disassembly of the LED
 * fixture was done.
 * 
 * Includes support for OTA firmware updates since this device
 * will likely end up stuck under your tank someplace.
 * 
 */

#include <math.h>
#include <ArduinoJson.h>
#include <Time.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <IRremoteESP8266.h>
#include <Dusk2Dawn.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SSD1306.h>
#include <OLEDDisplayUi.h>
#include "SSD1306Brzo.h"
#include "SH1106Brzo.h"

#include "buttons.h"
#include "font.h"
#include "images.h"
 

// Customize these to match your install
const char* ssid        = "private_network";
const char* password    = "seekret";
const char* weatherHost = "api.wunderground.com";
const char* state       = "MO";
const char* city        = "Blue_Springs";
const char* api_key     = "getyour0wnkey";
const float latitude    = 39.02000046;
const float longitude   = -94.27999878;
const signed int offset = -6;
const bool is_dst       = true;

struct SunTimes {
  TimeElements sunrise;
  TimeElements sunset;
};

struct Weather {
  const char* observation;
  float temp_f;
};

// Globals 
TimeElements sunrise; // These are LOCAL time not GMT
TimeElements sunset;  // because they are build by adding an offset to the current date/time
float last_temp;      // save this so I don't eat my API calls 
const char * last_observation;

// Water temp reading

#define ONE_WIRE_BUS 2 // Data wire is plugged into GPIO2 on the ESP
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress waterThermometer;

SSD1306  display(0x3c, D3, D5);
OLEDDisplayUi ui     ( &display );

static char respBuf[4096];

IRsend irsend(4); // init a sender on GPIO4

#define AIR_TEMP_PIN 0 // TMP 36 is on analog 0

String twoDigits(int digits){
  if(digits < 10) {
    String i = '0'+String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}

void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  String text;
  text += String(hourFormat12());
  text += ":";
  text += twoDigits(minute());
  text += ":";
  text += twoDigits(second());
  text += " ";
  if (isAM()) text += "AM  ";
  else text += "PM  ";
  text += monthShortStr(month());
  text += " ";
  text += day();
  text += " ";
  text += year();
  display->drawString(128, 0, text);
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Draw the water temperature icon 
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(Open_Sans_12);

  float tempC = sensors.getTempC(waterThermometer);
  String text;
  text += "Water Temp: ";
  text += DallasTemperature::toFahrenheit(tempC);
  display->drawString(0,20, text);
  
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Draw the outside temperature icon
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(Open_Sans_12);

  String text;
  text += "Outside Temp: ";
  text += last_temp;
  text += "\n";
  text += last_observation;
  display->drawString(0,20, text);
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Draw the inside temperature icon
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(Open_Sans_12);

  String text;
  text += "Inside Temp: ";
  text += getAirTemp();
  display->drawString(0,20, text);
}

FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3 };

// how many frames are there?
int frameCount = 3;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { clockOverlay };
int overlaysCount = 1;
 
void setup()
{
        Serial.begin(115200);
        irsend.begin();
        Alarm.delay(100); // always use this delay function so the alarms work
        yield();
        WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED) {
                Alarm.delay(500);
                yield();
                Serial.println(".");
        }

        Serial.println("");
        Serial.println(F("WiFi connected"));
        Serial.println(F("IP address: "));
        Serial.println(WiFi.localIP());

        // Use WiFiClient class to create TCP connections
        WiFiClient client;
        unsigned long unixTime = webUnixTime(client);
        Serial.print(F("Current unix time (GMT): ")); Serial.println(unixTime);
        setTime(unixTime); // set the time in the time library to the real time (in GMT)
        if (!is_dst) adjustTime(offset * 60 * 60); // adjust time to the local time so from now on all time refs are local
        else adjustTime((offset+1) *60 *60); // this probably breaks if you are +GMT. sorry.
        Serial.print(F("Adjusted unix time (local): ")); Serial.println(now());

        MorningAlarm(); // run once for setup of sunrise/sunset alarms the first time the app runs
                        // also sets the current lighting situation to match the time so it's correct
                        // on first run.  
        
        Alarm.alarmRepeat(5,0,0, MorningAlarm); // at 5:00AM update sunrise/sunset times
        Alarm.timerRepeat(900, quarterAlarm); // every 15 minute tasks - currently all weather related
        quarterAlarm(); // again run once to throw weather effects in place right away on device startup
 
        // Set up OTA firmware updates.
        
        ArduinoOTA.onStart([]() {
          Serial.println("Start OTA download");
        });
        ArduinoOTA.onEnd([]() {
          Serial.println("\nEnd OTA download");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });
        ArduinoOTA.begin();

        Serial.print("Locating devices...");
        sensors.begin();
        Serial.print("Found ");
        Serial.print(sensors.getDeviceCount(), DEC);
        Serial.println(" devices.");
        if (!sensors.getAddress(waterThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
        sensors.setResolution(waterThermometer, 9);

        ui.setTargetFPS(60);
        ui.setActiveSymbol(activeSymbol);
        ui.setInactiveSymbol(inactiveSymbol);
        ui.setIndicatorPosition(BOTTOM);
        ui.setIndicatorDirection(LEFT_RIGHT);
        ui.setFrameAnimation(SLIDE_LEFT);
        ui.setOverlays(overlays, overlaysCount);
        ui.setFrames(frames, frameCount);
        ui.init();

        display.flipScreenVertically();
          
}

void loop() {
        int remainingTimeBudget = ui.update();
      
        if (remainingTimeBudget > 0) {
          sensors.requestTemperatures();
          Alarm.delay(remainingTimeBudget);
        }
        ArduinoOTA.handle();
        yield();
}

void quarterAlarm() { // every 15 minute task - check the weather and change the lighting to match
        Weather weather;
        getWeather(weather);

        SunTimes times;
        getSunrise(times); 

        // TODO:
        //  figure out what the other observations are and map them.

        Serial.println(weather.observation);
        last_temp = weather.temp_f;
        last_observation = weather.observation;

        if (makeTime(times.sunrise) < now() && makeTime(times.sunset) > now()) // only change weather effects between sunrise and sunset
        {
          Serial.println("In weather update loop, It's during the day.");
          if (weather.observation == "Partly Cloudy") 
              irsend.sendNEC(btn_partlycloudy, 32);
          
          if (weather.observation == "Mostly Cloudy")
              irsend.sendNEC(btn_mostlycloudy, 32);

          if (weather.observation == "Overcast")
              irsend.sendNEC(btn_cloudy, 32);
           
          if (weather.observation == "Clear")
              irsend.sendNEC(btn_blue, 32); 
              
        } else {
          Serial.println("Not updating weather, it's dark.");
        }
}

void MorningAlarm() {
        // update sunrise/sunset alarms each Morning
        SunTimes times;
        getSunrise(times); 

        Serial.println(F("In MorningAlarm."));
        
        Serial.print(F("Sunrise: ")); Serial.print(times.sunrise.Hour); Serial.print(":");
        Serial.println(times.sunrise.Minute);
        Serial.print(F("Sunset: ")); Serial.print(times.sunset.Hour); Serial.print(":");
        Serial.println(times.sunset.Minute);
        
        Alarm.triggerOnce(makeTime(times.sunrise), alarmSunrise);  // sunrise button
        Alarm.triggerOnce(makeTime(times.sunrise) + 3600, alarmDay);  // full light
        Alarm.triggerOnce(makeTime(times.sunset), alarmDusk);        // dusk button
        Alarm.triggerOnce(makeTime(times.sunset) + 3600, alarmNight);   // night button
}

void getSunrise(SunTimes& times) {
  
        Dusk2Dawn blueSprings(latitude, longitude, offset);
     
        int sunrise_mins = blueSprings.sunrise(year(), month(), day(), is_dst);
        int sunset_mins  = blueSprings.sunset(year(), month(), day(), is_dst);

        Serial.print(F("Sunrise offset: ")); Serial.println(sunrise_mins);
        Serial.print(F("Sunset offset: ")); Serial.println(sunset_mins);

        TimeElements tm_sunrise;
        TimeElements tm_sunset;

        breakTime(now(), tm_sunrise);
        breakTime(now(), tm_sunset);
        
        tm_sunrise.Hour = sunrise_mins / 60;
        tm_sunrise.Minute = sunrise_mins % 60;

        tm_sunset.Hour = sunset_mins / 60;
        tm_sunset.Minute = sunset_mins % 60;
  
        times.sunrise = tm_sunrise;
        times.sunset = tm_sunset;
        
        Serial.print(F("Sunrise time: ")); Serial.println(makeTime(times.sunrise));
        Serial.print(F("Sunset time: ")); Serial.println(makeTime(times.sunset));
}

void getWeather(Weather& weather) {
        Serial.print(F("Getting weather at ")); Serial.println(now());
        // Use WiFiClient class to create TCP connections
        WiFiClient client;
        const int httpPort = 80;
        if (!client.connect(weatherHost, httpPort)) {
                Serial.println(F("connection failed"));
                return;
        }

        // We now create a URI for the request
        String url = "/api/";
        url += api_key;
        url += "/conditions/q/";
        url += state;
        url += "/";
        url += city;
        url += ".json";
        Serial.print(F("Requesting URL: "));
        Serial.println(url);

        // This will send the request to the server
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + weatherHost + "\r\n" +
                     "Connection: close\r\n\r\n");
        client.flush();
        Alarm.delay(500);

        // Collect http response headers and content from Weather Underground
        // HTTP headers are discarded.
        // The content is formatted in JSON and is left in respBuf.
        int respLen = 0;
        bool skip_headers = true;
        while (client.connected() || client.available()) {
                if (skip_headers) {
                        String aLine = client.readStringUntil('\n');
                        //Serial.println(aLine);
                        // Blank line denotes end of headers
                        if (aLine.length() <= 1) {
                                skip_headers = false;
                        }
                }
                else {
                        int bytesIn;
                        bytesIn = client.read((uint8_t *)&respBuf[respLen], sizeof(respBuf) - respLen);
                        //Serial.print(F("bytesIn ")); Serial.println(bytesIn);
                        if (bytesIn > 0) {
                                respLen += bytesIn;
                                if (respLen > sizeof(respBuf)) respLen = sizeof(respBuf);
                        }
                        else if (bytesIn < 0) {
                                Serial.print(F("read error "));
                                Serial.println(bytesIn);
                        }
                }
                delay(1);
        }
        client.stop();

        if (respLen >= sizeof(respBuf)) {
                Serial.print(F("respBuf overflow "));
                Serial.println(respLen);
                return;
        }

        // Terminate the C string

        respBuf[respLen++] = '\0';
        Serial.print(F("respLen "));
        Serial.println(respLen);

        char *jsonstart = strchr(respBuf, '{');
        if (jsonstart == NULL) {
                Serial.println(F("JSON data missing"));
                return;
        }

        char* json = jsonstart;

        const size_t bufferSize = JSON_OBJECT_SIZE(0) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(55) + 2300;
        DynamicJsonBuffer jsonBuffer(bufferSize);

        JsonObject& root = jsonBuffer.parseObject(json);
        JsonObject& response = root["response"];
        int response_features_conditions = response["features"]["conditions"]; // 1
        JsonObject& current_observation = root["current_observation"];
        weather.observation = current_observation["weather"]; // "Partly Cloudy"
        weather.temp_f = current_observation["temp_f"]; // 66.3

}

float getAirTemp() {
   int reading = analogRead(AIR_TEMP_PIN);  
   
   // converting that reading to voltage, for 3.3v arduino use 3.3
   float voltage = reading * 3.3;
   voltage /= 1024.0; 
   
   // print out the voltage
   Serial.print(voltage); Serial.println(" volts");
   
   // now print out the temperature
   float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                                 //to degrees ((voltage - 500mV) times 100)
   Serial.print(temperatureC); Serial.println(" degrees C");
   
   // now convert to Fahrenheit
   float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
   Serial.print(temperatureF); Serial.println(" degrees F");

   return temperatureF;
}

void alarmSunrise() {
  Serial.println(F("It's sunrise"));
  irsend.sendNEC(btn_orange, 32);
}

void alarmDay() {
  Serial.println(F("It's daytime"));
  irsend.sendNEC(btn_blue, 32);
}

void alarmDusk() {
  Serial.println(F("It's dusk"));
  irsend.sendNEC(btn_dusk, 32);
}

void alarmNight() {
  Serial.println(F("It's nighttime"));
  irsend.sendNEC(btn_night, 32);
}






/*
 * © Francesco Potortì 2013 - GPLv3
 *
 * Send an HTTP packet and wait for the response, return the Unix time
 */

unsigned long webUnixTime (Client &client)
{
        unsigned long time = 0;

        // Just choose any reasonably busy web server, the load is really low
        if (client.connect("www.google.com", 80))
        {
                // Make an HTTP 1.1 request which is missing a Host: header
                // compliant servers are required to answer with an error that includes
                // a Date: header.
                client.print(F("GET / HTTP/1.1 \r\n\r\n"));

                char buf[5]; // temporary buffer for characters
                client.setTimeout(5000);
                if (client.find((char *)"\r\nDate: ") // look for Date: header
                    && client.readBytes(buf, 5) == 5) // discard
                {
                        unsigned day = client.parseInt(); // day
                        client.readBytes(buf, 1); // discard
                        client.readBytes(buf, 3); // month
                        int year = client.parseInt(); // year
                        byte hour = client.parseInt(); // hour
                        byte minute = client.parseInt(); // minute
                        byte second = client.parseInt(); // second

                        int daysInPrevMonths;
                        switch (buf[0])
                        {
                        case 'F': daysInPrevMonths =  31; break;// Feb
                        case 'S': daysInPrevMonths = 243; break; // Sep
                        case 'O': daysInPrevMonths = 273; break; // Oct
                        case 'N': daysInPrevMonths = 304; break; // Nov
                        case 'D': daysInPrevMonths = 334; break; // Dec
                        default:
                                if (buf[0] == 'J' && buf[1] == 'a')
                                        daysInPrevMonths = 0; // Jan
                                else if (buf[0] == 'A' && buf[1] == 'p')
                                        daysInPrevMonths = 90; // Apr
                                else switch (buf[2])
                                        {
                                        case 'r': daysInPrevMonths =  59; break;// Mar
                                        case 'y': daysInPrevMonths = 120; break; // May
                                        case 'n': daysInPrevMonths = 151; break; // Jun
                                        case 'l': daysInPrevMonths = 181; break; // Jul
                                        default: // add a default label here to avoid compiler warning
                                        case 'g': daysInPrevMonths = 212; break; // Aug
                                        }
                        }

                        // This code will not work after February 2100
                        // because it does not account for 2100 not being a leap year and because
                        // we use the day variable as accumulator, which would overflow in 2149
                        day += (year - 1970) * 365; // days from 1970 to the whole past year
                        day += (year - 1969) >> 2; // plus one day per leap year
                        day += daysInPrevMonths; // plus days for previous months this year
                        if (daysInPrevMonths >= 59 // if we are past February
                            && ((year & 3) == 0)) // and this is a leap year
                                day += 1; // add one day
                        // Remove today, add hours, minutes and seconds this month
                        time = (((day-1ul) * 24 + hour) * 60 + minute) * 60 + second;
                }
        }
        delay(10);
        client.flush();
        client.stop();

        return time;
}
