#include <ESP8266mDNS.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFiScan.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <IRremoteESP8266.h>

#include "buttons.h"

IRsend irsend(0);

void setup()
{
  irsend.begin();
}

void loop() {
  irsend.sendNEC(orange, 32);
  delay(2000);
  irsend.sendNEC(blue, 32);
  delay(2000);
}

