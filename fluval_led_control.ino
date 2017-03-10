#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <IRremoteESP8266.h>

#include "buttons.h"

const char* ssid     = "myssid";
const char* password = "mypassword";

const char* host = "api.wunderground.com";
const char* state = "MO";
const char* city = "Blue_Springs";
const char* api_key = "myapikey";

IRsend irsend(0);

void setup()
{
  Serial.begin(115200);
  irsend.begin();
  delay(100);
  WiFi.begin(ssid, password);

 while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
 }

 Serial.println("");
 Serial.println("WiFi connected");
 Serial.println("IP address: ");
 Serial.println(WiFi.localIP());
}

void loop() {
  // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
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
    Serial.print("Requesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    delay(500);

    // Read all the lines of the reply from server and print them to Serial
    while(client.available()){
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }

    // extract the relative amount of cloudiness and lightning to use
    // as well as the time (2 pieces of info, one API call!)



    irsend.sendNEC(orange, 32);
    delay(2000);
    irsend.sendNEC(blue, 32);
    delay(150000);
}
