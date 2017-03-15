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
* Breadboard, stripboard, or clever soldering skills
* Optionally a 3d printer to make an enclosure - I used [this one](http://www.thingiverse.com/thing:1128026)

Install the application with the Arduino IDE. You will need to install
the ESP8266 board support package through board manager, and the ESP8266
libraries for IR, WiFi, Time, TimeAlarms, and Dusk2Dawn through the library manager.

Note that installing a program via the Arduino IDE will blow away the Lua
environment that comes on the module. It's possible to restore if you like;
IMO it is a less capable solution than using the Arduino IDE anyway
and I doubt you will miss it.

# Wiring

Wiring is simple - the LED needs to be connected to GPIO0. On the module I used, that pin is labeled as D3; check the pinout of your individual module
to be sure. Connect the resistor to the GPIO pin, the other leg of the
resistor to the positive leg of the IR LED, and the negative leg of the LED
to ground.

# Usage

Configure the variables for your location, wireless SSID, etc. and flash to the module.
Mount it someplace where the IR LED will illuminate the IR receiver attached to the
light fixture.

# Caveats

As of this push the weather functionality is half baked - only a few possible
conditions actually equate to a lighting scene. Also weather conditions will override
time of day which is not ideal when the weather changes to clear at 11:00pm
and the lights suddenly come on full blast again.

Both of these are marked with TODO and I'll get to them as soon as I can.
