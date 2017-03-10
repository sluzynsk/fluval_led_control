# Fluval LED control for Fluval Aquasky

# Description

I acquired a Fluval Aquasky LED light for my aquarium. It comes with a
remote control with buttons for color temperature as well as for
various time of day and weather effects.

While pushing the button on the remote is cool, it seemed that it would be
even cooler to have the lights change on their own. Out of that notion
this application was born.

The application will change the lights in the aquarium to match the current
time of day. Optionally weather effects can be enabled; this is a configurable
option because the lightning flashes may not appeal to you.

# Installation

To install the application, you will need:

* The Arduino IDE
* An ESP8266 board, such as a [NodeMCU](https://smile.amazon.com/gp/product/B010O1G1ES/ref=oh_aui_detailpage_o00_s00?ie=UTF8&psc=1).
* One IR LED
* One 330 ohm resistor
* Breadboard, stripboard, or just clever soldering skills

Install the application with the Arduino IDE. You will need to install
the ESP8266 board support package through board manager, and the ESP8266
libraries for IR, WiFi, and web server through the library manager.

Note that installing a program via the Arduino IDE will blow away the Lua
environment that comes on the module. It's possible to restore if you like;
IMO it is a less capable solution than using the Arduino toolset anyway
and I doubt you will miss it. 

# Wiring

Wiring is simple - the LED needs to be connected to GPIO0. On the module I used, that pin is labeled as D3; check the pinout of your individual module
to be sure. Connect the resistor to the GPIO pin, the other leg of the
resistor to the positive leg of the IR LED, and the negative leg of the LED
to ground.

# Usage

On first boot, the ESP will boot up in web server mode. Locate the SSID
for the ESP on your phone or laptop and join it.
