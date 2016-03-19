#ESP8266 temperature sensor

This folder contains all the code and schematics for the ESP8266 MQTT Temperature Sensor board.

The board uses the DS1631 I2C temperature sensor IC, but it would be easily modified for whateverboard temperature sensor you want.
I'm still developing this, so the code might be a bit buggy at the moment.

## 19 March 2016
Added a rotary encoder to the system.
* This allow the desired temperature to be set via the wall mounted system.
* The desired temperature is then shown on the OLED and sent via MQTT.
* If the desired temperature changes in the node-red controller, the new deisred temperature is sent to the ESP8266 via MQTT and the new desired temperature is shown.
* FSM updated to include rotary encoder temperature changes.

## 28 Feb 2016
Added a 0.96" OLED and a FSM to the project.
*	The OLED will display the current temperature, which is updated every 60 seconds.
*	The VERY simple FSM trigger is used to obtain the current temperature (more details available soon).

## 21 Feb 2016
Created seperate config file to hold passwords, etc. for wifi and MQTT broker.
This version is now usable as a simple MQTT temperature sensor by modifying the config file as necessary.
