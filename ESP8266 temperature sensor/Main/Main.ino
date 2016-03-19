/*
  ESP8266 MQTT Temperature Sensor with OLED and rotary encoder.
  18 March 2016
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
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
const long interval = 1000*60;           // interval at which to blink (milliseconds)

// Rotary encoder
//const int bounceValue = 50; // ms
int previous = 0;
const int PinCLK = 5;     // Used for generating interrupts using CLK signal
const int PinDT = 4;     // Used for reading DT signal
//const int PinSW = 8;     // Used for the push button switch. Not currently used.

double desiredTemp = 20.0;
double stepValue = 0.5;
double maxTemperature = 30;
double minTemperature = 10;
double T_dec = 0.0;

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


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(esp8266_client)){//, mqtt_username, mqtt_password)) {
      Serial.println(" MQTT connected");
      // Subscribing
      client.subscribe(mqtt_desired_temp_receive);
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
#define DESIRED_TEMP_UPDATE 2

void on_waiting_enter(){
  Serial.println("Entering waiting.");
}

void on_waiting_exit(){
  Serial.println("Leaving waiting");
}

State state_waiting(on_waiting_enter, &on_waiting_exit);
Fsm fsm(&state_waiting);

void transition_new_desired_temp() {
  Serial.println("Setting new desired temperature");

  Serial.println(desiredTemp);
  
  display.setCursor(0, 0);
  display.setTextSize(3);
  printDouble(T_dec, 1);
  display.setCursor(50, 40);
  display.setTextSize(2);
  printDouble(desiredTemp, 1);
  display.display();
  //ssddelay(2000);
  display.clearDisplay();

  int DT = desiredTemp * 2;

  snprintf (msg, 75, "%d", DT);
  client.publish(mqtt_desired_temp_send, msg);
}

void transition_getting_temperature() {
  Serial.println("Obtaining temperature...");

  uint16_t temp = Temp1.readTempOneShotInt();
  while (!Temp1.conversionDone()) {}

  uint8_t Th = temp >> 8;
  uint8_t Tl = temp & 0xFF;

  if (Th >= 0x80)     //if sign bit is set, then temp is negative
    Th = Th - 256;
  temp = (uint16_t)((Th << 8) + Tl);
  temp >>= 4;
  T_dec = temp * 0.0625;

  // Display T° on "Serial Monitor"
  Serial.print("Temperature : ");
  Serial.println(T_dec);

  // Sending the unconverted temperature via MQTT
  snprintf (msg, 75, "%d", temp);
  client.publish(mqtt_topic, msg);

  // Printing Temperature
  display.setCursor(0, 0);
  display.setTextSize(3);
  printDouble(T_dec, 1);
  display.setCursor(50, 40);
  display.setTextSize(2);
  printDouble(desiredTemp, 1);
  display.display();
  //delay(2000);
  display.clearDisplay();
}

void setup_fsm(){
  Serial.println("Setting up FSM.");
  fsm.add_transition(&state_waiting, &state_waiting, TEMPERATURE_TIME_TRIGGER, &transition_getting_temperature);
  fsm.add_transition(&state_waiting, &state_waiting, DESIRED_TEMP_UPDATE, &transition_new_desired_temp);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting system");

  
  pinMode(PinCLK,INPUT);
  pinMode(PinDT, INPUT);
  //pinMode(PinSW, INPUT);    

  setup_oled();
  setup_fsm();
  setup_ds1631();
  setup_wifi();

  fsm.trigger(TEMPERATURE_TIME_TRIGGER);
}

char message_buff[100];
// Used for when the ESP board receives a message it has subscribed to. Blank for now as not subscribed to anything.
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Received something");
  for (int i =0 ; i < length; i++){
  Serial.print(payload[i]);}
  Serial.println();

 // create character buffer with ending null terminator (string)
  for(int i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
    message_buff[length+1] = '\0';
   String msgString = String(message_buff);
   Serial.println("Payload: " + msgString);
 

  if (strcmp(topic, mqtt_desired_temp_receive) == 0){
    desiredTemp = msgString.toInt() / 2;
    fsm.trigger(DESIRED_TEMP_UPDATE);
  }
}


#define NUMBUTTONS 2
volatile uint8_t pressed[NUMBUTTONS], justPressed[NUMBUTTONS], justReleased[NUMBUTTONS], timePressed[NUMBUTTONS];

//#define NUMBUTTONS 6
#define CHARGE PD3
#define OFF PD2
#define UP PD1
#define DOWN PD0
#define CLK PD5
#define SELECT PD6
#define ROTARY 0

int bounceValue = 5;

void checkForButtonPress(){
  //Serial.println("Checking");
  static uint8_t previousState[NUMBUTTONS];
  static uint8_t currentState[NUMBUTTONS];
  static uint16_t lastTime;
  uint8_t index;
  //uint8_t input = PIND;
  uint8_t input[2]= {digitalRead(PinCLK), digitalRead(PinDT)};
  //uint8_t inputCheck[] = {OFF, CHARGE, UP, DOWN, CLK, SELECT};
  uint8_t inputCheck[] = {0, 1};
  
  if (lastTime++ < bounceValue){ // 15ms * 4 debouncing time
    return;
  }
  
  lastTime = 0;
  for (index = 0; index < NUMBUTTONS; index++){   
    if (!input[index])
      currentState[index] = 0;
    else
      currentState[index] = 1;
    
    if (currentState[index] == previousState[index]) {
      if ((pressed[index] == 0) && (currentState[index] == 0)) {
      // just pressed
      justPressed[index] = 1;
      timePressed[index] = 0;
      }
      else if ((pressed[index] == 1) && (currentState[index] == 1)) {
        // just released
        justReleased[index] = 1;
      }
      pressed[index] = !currentState[index];  // remember, digital HIGH means NOT pressed
      if (pressed[index])
        timePressed[index]++;
    }
    previousState[index] = currentState[index];   // keep a running tally of the buttons
  } 
}

void checkForRotaryChange(){
  static int order[] = {0,0,0,0};
  static int order2[] = {0,0,0,0};
  
  if (justPressed[0] || justPressed[1]){
    for (int i = 0; i < 3; i++)
      order[i] = order[i+1];
    order[3] = pressed[0];
    for (int i = 0; i < 3; i++)
      order2[i] = order2[i+1];
    order2[3] = pressed[1];
    justPressed[0] = 0;
    justPressed[1] = 0;
  }
  else if (justReleased[0] || justReleased[1]){
    for (int i = 0; i < 3; i++)
      order[i] = order[i+1];
    order[3] = pressed[0];
    for (int i = 0; i < 3; i++)
      order2[i] = order2[i+1];
    order2[3] = pressed[1];
    justReleased[0] = 0;
    justReleased[1] = 0;

    if (order[0] == 0 && order[1] == 1 && order[2] == 1 && order[3] == 0){
      if (order2[0] == 1 && order2[1] == 1 && order2[2] == 0 && order2[3] == 0){
        if ((desiredTemp += stepValue) > maxTemperature)
          desiredTemp = maxTemperature;
        Serial.println(desiredTemp);
        fsm.trigger(DESIRED_TEMP_UPDATE);
      }
    }
    else if (order[0] == 1 && order[1] == 1 && order[2] == 0 && order[3] == 0){
      if (order2[0] ==  0 && order2[1] == 1 && order2[2] == 1 && order2[3] == 0){
        if ((desiredTemp -= stepValue) < minTemperature)
          desiredTemp = minTemperature;
        Serial.println(desiredTemp);
        fsm.trigger(DESIRED_TEMP_UPDATE);
      }
    }
  }   
}

void loop() {
  Serial.println("Getting started");
  unsigned long currentMillis = millis();

  // Checking if time to get new temperature
  if (currentMillis - previousMillis2 > interval){
    fsm.trigger(TEMPERATURE_TIME_TRIGGER);
    previousMillis2 = currentMillis;
  }
  // Checking if time to get new rotary encoder value
  else if (currentMillis - previousMillis >= 1) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    
    checkForButtonPress();
    checkForRotaryChange();
  }

  // Keeping the client connected.
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
}