/**************************
 * Fluval LED Control
 * for ESP8266-12E (NodeMCU) modules
 * (C)2017 by Steve Luzynski except where noted 
 * 
 * Wiring: Connect an IR LED to GPIO0 through a 330 Ohm resistor to the 3.3V 
 * power supply pin.
 * 
 * Codes in buttons.h reversed by pointing the OEM remote at an
 * ESP based IR decoder module, no disassembly of the LED
 * fixture was done.
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

#include "buttons.h"

#define WEATHER // define this to enable weather interactivity or
                // comment it out if you wish to disable the weather

// Customize these to match your install
const char* ssid        = "privat_network";
const char* password    = "secret";
const char* weatherHost = "api.wunderground.com";
const char* state       = "MO";
const char* city        = "Blue_Springs";
const char* api_key     = "getyourownkey";
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
};

// Globals 
TimeElements sunrise; // These are LOCAL time not GMT
TimeElements sunset;  // because they are build by adding an offset to the current date/time

static char respBuf[4096];

IRsend irsend(0); // init a sender
 
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
        else adjustTime((offset+1) *60 *60);
        Serial.print(F("Adjusted unix time (local): ")); Serial.println(now());

        MorningAlarm(); // run once for setup of sunrise/sunset alarms the first time the app runs
                        // also sets the current lighting situation to match the time so it's correct
                        // on first run.  
        
        Alarm.alarmRepeat(5,0,0, MorningAlarm); // at 5:00AM update sunrise/sunset times
        #ifdef WEATHER
        Alarm.timerRepeat(900, quarterAlarm); // every 15 minute tasks - currently all weather related
        #endif
}

void loop() {
        Alarm.delay(1000);
        yield();
}

void quarterAlarm() { // every 15 minute task - check the weather and change the lighting to match
        Weather weather;
        getWeather(weather);

        // TODO:
        // Item 1 - make sure it doesn't change the weather lighting after dusk (dusk trumps weather)
        // Item 2 - figure out what the other observations are and map them.

        Serial.println(weather.observation);
        if (weather.observation == "Partly Cloudy") 
            irsend.sendNEC(btn_partlycloudy, 32);
        
        if (weather.observation == "Mostly Cloudy")
            irsend.sendNEC(btn_mostlycloudy, 32);
         
        if (weather.observation == "Clear")
            irsend.sendNEC(btn_blue, 32); // this is probably a bad choice, fix TODO
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

        // compare current time to sunrise/sunset times and set the lights as appropriate.
        // this will fix the first run situation. on subsequent runs it will do nothing as the lights
        // will already be correct.
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
