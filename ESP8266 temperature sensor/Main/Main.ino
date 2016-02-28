/*
  DS1631_MQTT_Temperature_Sensor_with_OLED
  27 Feb 2016
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Wire.h>
#include <SPI.h>
#include <DS1631.h>
#include <ESP_SSD1306.h>    // Modification of Adafruit_SSD1306 for ESP8266 compatibility
#include <Adafruit_GFX.h>   // Needs a little change in original Adafruit library (See README.txt file)
#include <Fsm.h>

#include "Config.h"         // Contains all the SSID and MQTT config data (such as usernames, passwords, and topics).

// "Random" global variables
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000*60;           // interval at which to blink (milliseconds)

// Wifi
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// Used for when the ESP board receives a message it has subscribed to. Blank for now as not subscribed to anything.
void callback(char* topic, byte* payload, unsigned int length) {
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(esp8266_client, mqtt_username, mqtt_password)) {
      Serial.println(" MQTT connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Temperature sensor
DS1631 Temp1(0);
char msg[5];
void setup_ds1631(){
  // Setting up DS1631
  Serial.println("Setting up DS1631.");
  //Wire.begin(2, 14);
  int config = Temp1.readConfig();
  Serial.print("Config settings before: ");
  Serial.println(config, BIN);

  Temp1.writeConfig(13);
  config = Temp1.readConfig();
  Serial.print("Config settings after: ");
  Serial.println(config, BIN);
}

// OLED
#define OLED_RESET  16  // Pin 15 -RESET digital signal
ESP_SSD1306 display(OLED_RESET); // FOR I2C
void setup_oled(){
  Serial.println("Setting up the OLED.");
  display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED
  display.clearDisplay();
  display.setTextColor(WHITE);  
  display.display();
}

void printDouble( double val, byte precision){
 // prints val with number of decimal places determine by precision
 // precision is a number from 0 to 6 indicating the desired decimial places
 // example: printDouble( 3.1415, 2); // prints 3.14 (two decimal places)

 display.print (int(val));  //prints the int part
 if( precision > 0) {
   display.print("."); // print the decimal point
   unsigned long frac;
   unsigned long mult = 1;
   byte padding = precision -1;
   while(precision--)
      mult *=10;
      
   if(val >= 0)
     frac = (val - int(val)) * mult;
   else
     frac = (int(val)- val ) * mult;
   unsigned long frac1 = frac;
   while( frac1 /= 10 )
     padding--;
   while(  padding--)
     display.print("0");
   display.print(frac,DEC) ;
 }
}

// FSM
// Transition callback function
#define TEMPERATURE_TIME_TRIGGER 1

void on_waiting_enter(){
  Serial.println("Entering waiting.");
}

void on_waiting_exit(){
  Serial.println("Leaving waiting");
}

State state_waiting(on_waiting_enter, &on_waiting_exit);
Fsm fsm(&state_waiting);

void transition_getting_temperature(){
  Serial.println("Obtaining temperature...");

  uint16_t temp = Temp1.readTempOneShotInt();
  while (!Temp1.conversionDone()){}
  
  uint8_t Th = temp >> 8;
  uint8_t Tl = temp & 0xFF;

  if(Th>=0x80)        //if sign bit is set, then temp is negative
    Th = Th - 256;
  temp = (uint16_t)((Th << 8) + Tl);
  temp >>= 4;
  float T_dec = temp * 0.0625;

  // Display T° on "Serial Monitor"
  Serial.print("Temperature : ");
  Serial.println(T_dec);

  // Sending the unconverted temperature via MQTT
  snprintf (msg, 75, "%d", temp);
  client.publish(mqtt_topic, msg);

  // Printing Temperature
  display.setCursor(0,0);
  display.setTextSize(3);
  printDouble(T_dec, 1);
  display.display();
  delay(2000);
  display.clearDisplay();
}

void setup_fsm(){
  Serial.println("Setting up FSM.");
  fsm.add_transition(&state_waiting, &state_waiting, TEMPERATURE_TIME_TRIGGER, &transition_getting_temperature);
}

void setup() {
  Serial.begin(115200);

  setup_oled();
  setup_fsm();
  setup_ds1631();
  setup_wifi();
}

void loop() {
  //Serial.println("Getting Temperature.");

  unsigned long currentMillis = millis();

  // Checking if time to get new temperature
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    fsm.trigger(TEMPERATURE_TIME_TRIGGER);
  }
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
