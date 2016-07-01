#/*
  Rotary switch_0.7
  17 March 2016
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <Ticker.h>
#include <Wire.h>
#include <SPI.h>
#include <DS1631.h>
#include <Fsm.h>
#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "Config.h"         // Contains all the SSID and MQTT config data (such as usernames, passwords, and topics).

// "Random" global variables
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
const long interval = 1000 * 60;         // interval at which to blink (milliseconds)
int encoderChange = 0;

// Rotary encoder
//const int bounceValue = 50; // ms
int previous = 0;
const int PinCLK = 5;     // Used for generating interrupts using CLK signal
const int PinDT = 4;     // Used for reading DT signal
const int PinSW = 16;     // Used for the push button switch.

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
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(esp8266_client)) { //, mqtt_username, mqtt_password)) {
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
void setup_ds1631() {
  // Setting up DS1631
  Serial.println("Setting up DS1631.");
  //Wire.begin(12, 13);
  int config = Temp1.readConfig();
  Serial.print("Config settings before: ");
  Serial.println(config, BIN);

  Temp1.writeConfig(13);
  config = Temp1.readConfig();
  Serial.print("Config settings after: ");
  Serial.println(config, BIN);
}

// OLED

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306   display(0x3c, 13, 12);
SSD1306Ui ui     ( &display );

void setup_oled() {
  Serial.println("Setting up the OLED.");

  ui.setTargetFPS(30);
  // Inital UI takes care of initalising the display too.
  ui.init();
  

  display.flipScreenVertically();
  display.clear();
  display.drawString(10, 10, "Starting up...");
  display.display();
}



char *ftoa(char *a, double f, int precision)
{
 long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
 
 char *ret = a;
 long heiltal = (long)f;
 itoa(heiltal, a, 10);
 while (*a != '\0') a++;
 *a++ = '.';
 long desimal = abs((long)((f - heiltal) * p[precision]));
 itoa(desimal, a, 10);
 return ret;
}

void updateOled(){

  display.clear();

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Living Room");
  
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  // Actual Temp
  ftoa(msg, T_dec, 1);
  display.drawString(curTempLocationX, curTempLocationY, msg);
  ftoa(msg, desiredTemp, 1);
  display.drawString(desTempLocationX, desTempLocationY, msg);

  display.setFont(ArialMT_Plain_10);
  display.drawString(curTempLocationX, curTempLocationY+20, "Current");
  display.drawString(desTempLocationX, desTempLocationY+20, "Desired");

  int curW = display.getStringWidth("Current");
  int desW = display.getStringWidth("Desired");

  display.drawRect(curTempLocationX-(curW/2), curTempLocationY+19, curW, 0);
  display.drawRect(desTempLocationX-(desW/2), desTempLocationY+19, desW, 0);
  
  display.display();
}

// FSM
// Trigger definitions
#define TRIGGER_TEMPERATURE_WAIT 1
#define TRIGGER_NEW_MQTT_DESIRED_TEMP 2
#define TRIGGER_60_SECONDS 3
#define TRIGGER_ENCODER_CHANGE 4

void on_waiting_enter() {
  Serial.println("Entering waiting.");
}

void on_waiting_exit() {
  Serial.println("Leaving waiting");
}

void on_sleeping_enter() {
  Serial.println("Entering sleeping.");
}

void on_sleeping_exit() {
  Serial.println("Leaving sleeping");
}

State state_waiting(on_waiting_enter, &on_waiting_exit);
State state_sleeping(on_sleeping_enter, &on_sleeping_exit);
Fsm fsm(&state_waiting);

void transition_new_desired_temp_mqtt() {
  Serial.println("Received new desired temperature");
  Serial.println(desiredTemp);

  updateOled();
}

void transition_new_desired_temp_mqtt_sleeping() {
  Serial.println("Received new desired temperature");
  Serial.println(desiredTemp);
}

void transition_new_desired_temp() {
  Serial.println("Setting new desired temperature");

  if (encoderChange == 1) {
    if ((desiredTemp += stepValue) > maxTemperature)
      desiredTemp = maxTemperature;
  }
  else if (encoderChange == 2) {
    if ((desiredTemp -= stepValue) < minTemperature)
      desiredTemp = minTemperature;
  }
  Serial.println(desiredTemp);

  updateOled();

  int DT = desiredTemp * 2;

  snprintf (msg, 75, "%d", DT);
  client.publish(mqtt_desired_temp_send, msg);
}

void obtainTemperature() {
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
}

void obtainDisplayTemperature() {
  Serial.println("Obtaining temperature...");

  uint16_t temp = Temp1.readTempOneShotInt();
  while (!Temp1.conversionDone()) {}

  uint8_t Th = highByte(temp);
  uint8_t Tl = lowByte(temp);

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
  
  updateOled();
}

void transition_obtain_temp_display() {
  Serial.println("Obtaining and displaying temperature");
  obtainDisplayTemperature();
}

void transition_button_pressed() {
  Serial.println("Button has just been released");
}

void transition_waiting_to_sleeping() {
  Serial.println("Going to sleep");

  /*display.clear();
  display.drawString(0, 0, "");
  display.display();*/
  display.displayOff();
}

void transition_encoder_move_from_sleeping() {
  Serial.println("Going from sleep to wait");
  display.displayOn();
  updateOled();
}

void transition_obtain_temp() {
  Serial.println("Obtaining temperature without displaying it");
  obtainTemperature();
}

void setup_fsm() {
  Serial.println("Setting up FSM.");
  fsm.add_transition(&state_waiting, &state_sleeping, TRIGGER_60_SECONDS, &transition_waiting_to_sleeping);
  fsm.add_transition(&state_sleeping, &state_waiting, TRIGGER_ENCODER_CHANGE, &transition_encoder_move_from_sleeping);
  fsm.add_transition(&state_waiting, &state_waiting, TRIGGER_TEMPERATURE_WAIT, &transition_obtain_temp_display);
  fsm.add_transition(&state_sleeping, &state_sleeping, TRIGGER_TEMPERATURE_WAIT, &transition_obtain_temp);
  fsm.add_transition(&state_waiting, &state_waiting, TRIGGER_ENCODER_CHANGE, &transition_new_desired_temp);
  fsm.add_transition(&state_waiting, &state_waiting, TRIGGER_NEW_MQTT_DESIRED_TEMP, &transition_new_desired_temp_mqtt);
  fsm.add_transition(&state_sleeping, &state_sleeping, TRIGGER_NEW_MQTT_DESIRED_TEMP, &transition_new_desired_temp_mqtt_sleeping);
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting system");


  pinMode(PinCLK, INPUT);
  pinMode(PinDT, INPUT);
  pinMode(PinSW, INPUT);

  setup_oled();
  setup_fsm();
  setup_ds1631();
  setup_wifi();

  fsm.trigger(TRIGGER_TEMPERATURE_WAIT);
}

bool receivedDesiredTemp = false;

char message_buff[100];
// Used for when the ESP board receives a message it has subscribed to. Blank for now as not subscribed to anything.
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Received something");

  for (int i = 0 ; i < length; i++) {
    Serial.print(payload[i]);
  }
  Serial.println();

  // create character buffer with ending null terminator (string)
  for (int i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[length + 1] = '\0';
  String msgString = String(message_buff);
  Serial.println("Payload: " + msgString);


  if (strcmp(topic, mqtt_desired_temp_receive) == 0) {
    desiredTemp = msgString.toInt() / 2;
    receivedDesiredTemp = true;
    //    fsm.trigger(DESIRED_TEMP_UPDATE);
  }
  else {
    receivedDesiredTemp = false;
  }
}


#define NUMBUTTONS 3
uint8_t pressed[NUMBUTTONS], justPressed[NUMBUTTONS], justReleased[NUMBUTTONS], timePressed[NUMBUTTONS];

int bounceValue = 5;

void checkForButtonPress() {
  //Serial.println("Checking");
  static uint8_t previousState[NUMBUTTONS];
  static uint8_t currentState[NUMBUTTONS];
  static uint16_t lastTime;
  uint8_t index;
  uint8_t input[NUMBUTTONS] = {digitalRead(PinCLK), digitalRead(PinDT), digitalRead(PinSW)};
  uint8_t inputCheck[] = {0, 1};

  if (lastTime++ < bounceValue) { // 15ms * 4 debouncing time
    return;
  }

  lastTime = 0;
  for (index = 0; index < NUMBUTTONS; index++) {
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

int checkForRotaryChange() {
  static int order[] = {0, 0, 0, 0};
  static int order2[] = {0, 0, 0, 0};

  if (justReleased[2]){
     justReleased[2] = 0;
     return 3;
   }
  if (justPressed[0] || justPressed[1]) {
    for (int i = 0; i < 3; i++)
      order[i] = order[i + 1];
    order[3] = pressed[0];
    for (int i = 0; i < 3; i++)
      order2[i] = order2[i + 1];
    order2[3] = pressed[1];
    justPressed[0] = 0;
    justPressed[1] = 0;
  }
  else if (justReleased[0] || justReleased[1]) {
    for (int i = 0; i < 3; i++)
      order[i] = order[i + 1];
    order[3] = pressed[0];
    for (int i = 0; i < 3; i++)
      order2[i] = order2[i + 1];
    order2[3] = pressed[1];
    justReleased[0] = 0;
    justReleased[1] = 0;

    if (order[0] == 0 && order[1] == 1 && order[2] == 1 && order[3] == 0) {
      if (order2[0] == 1 && order2[1] == 1 && order2[2] == 0 && order2[3] == 0) {
        return 1;   // Clockwise
      }
    }
    else if (order[0] == 1 && order[1] == 1 && order[2] == 0 && order[3] == 0) {
      if (order2[0] ==  0 && order2[1] == 1 && order2[2] == 1 && order2[3] == 0) {
        return 2; //Anti-clockwise
      }
    }
  }
  return 0;
}

unsigned long waitingTime = 0;
unsigned long tempWaitingTime = 0;
unsigned long debounceTime = 0;
unsigned long previousMicros = 0;

void loop() {
  //Serial.println("Getting started");
  unsigned long currentMillis = millis();
  unsigned long currentMicros = micros();

  // Checking for triggers
  waitingTime += (currentMillis - previousMillis);
  debounceTime += (currentMicros - previousMicros);
  tempWaitingTime += (currentMillis - previousMillis);

  encoderChange = 0;

  if (debounceTime >= 500) {
    // Checking for RE changes.
    debounceTime = 0;
    checkForButtonPress();
    encoderChange = checkForRotaryChange();
  }

  if (waitingTime >= 60000) { // 10 seconds (to go to sleep)
    fsm.trigger(TRIGGER_60_SECONDS);
    waitingTime = 0;
  }
  if (encoderChange) {
    fsm.trigger(TRIGGER_ENCODER_CHANGE);
    waitingTime = 0;
  }
  if (tempWaitingTime >= 60000) {
    // Obtain the current temperature
    fsm.trigger(TRIGGER_TEMPERATURE_WAIT);
    tempWaitingTime = 0;
  }
  if (receivedDesiredTemp) {
    // New desired temp via MQTT
    fsm.trigger(TRIGGER_NEW_MQTT_DESIRED_TEMP);
    receivedDesiredTemp = false;
  }

  previousMillis = currentMillis;
  previousMicros = currentMicros;

  // Keeping the client connected.
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
