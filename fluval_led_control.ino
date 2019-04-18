/**************************
 * Fluval LED Control
 * for ESP8266-12E (NodeMCU) modules
 * (C)2017 by Steve Luzynski except where noted 
 * 
 * Wiring: 
 * 
 * IR Control: Connect an IR LED to GPIO4 through a transistor such as a 2N3904.
 * Honestly I just stuck a resisitor on it and connected it to ground but
 * technically you may be drawing too much current for the ESP and/or
 * underdriving the LED which could impair range. It works for me
 * without the transistor. You do you.
 * 
 * Codes in buttons.h reversed by pointing the OEM remote at an
 * ESP based IR decoder module, no disassembly of the LED
 * fixture was done.
 * 
 */

#include <math.h>
#include <Time.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <ESP8266WiFi.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Dusk2Dawn.h>

#include "buttons.h"
 

// Customize these to match your install
const char* ssid         = "private_network";
const char* password     = "leet_password";
const float latitude     = 39.02000046;
const float longitude    = -94.27999878;
const signed int offset  = -6;
const bool is_dst        = true;

struct SunTimes {
  TimeElements sunrise;
  TimeElements sunset;
};

// Globals 
TimeElements sunrise; // These are LOCAL time not GMT
TimeElements sunset;  // because they are build by adding an offset to the current date/time

IRsend irsend(4); // init a sender on GPIO4

void setup()
{
        Serial.begin(115200);
        irsend.begin();
        Alarm.delay(100); // always use this delay function so the alarms work
        yield();
        Serial.printf("Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode(WIFI_STA) ? "" : "Failed!");
        Serial.printf("Connecting to %s \n", ssid);
        WiFi.begin(ssid, password);
        Serial.setDebugOutput(true);

        while (WiFi.status() != WL_CONNECTED) {
                Alarm.delay(500);
                yield();
                Serial.println("Connecting...");
                Serial.printf("Connection status: %d\n", WiFi.status());
                if (WiFi.status() == WL_CONNECT_FAILED) {
                  WiFi.printDiag(Serial);
                  break;
                }
         
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
  
}

void loop() {

        Alarm.delay(100);
        yield();
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
