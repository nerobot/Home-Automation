#ESP8266 temperature sensor

This folder contains all the code and schematics for the ESP8266 MQTT Temperature Sensor board.

The board uses the DS1631 I2C temperature sensor IC, but it would be easily modified for whateverboard temperature sensor you want.
I'm still developing this, so the code might be a bit buggy at the moment.


## 21 Feb 2016
Created seperate config file to hold passwords, etc. for wifi and MQTT broker. This version is now usable as a simple MQTT temperature sensor by modifying the config file as necessary.
