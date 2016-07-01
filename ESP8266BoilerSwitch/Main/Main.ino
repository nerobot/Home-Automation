/*
  Rotary switch_0.7
  17 March 2016
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "Config.h"         // Contains all the SSID and MQTT config data (such as usernames, passwords, and topics).

int boilerState = 0;
#define boilerSwitchPin D1

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
      client.subscribe(mqtt_receive_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting system");

  pinMode(boilerSwitchPin, OUTPUT);
  setup_wifi();
}


char message_buff[100];
// Used for when the ESP board receives a message it has subscribed to. Blank for now as not subscribed to anything.
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Received something");
  
  // create character buffer with ending null terminator (string)
  for (int i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[length + 1] = '\0';
  String msgString = String(message_buff);
  Serial.println("Payload: " + msgString);


  if (strcmp(topic, mqtt_receive_topic) == 0) {
    //desiredTemp = msgString.toInt() / 2;
    if (msgString.toInt() == 1){
      boilerState = 1;
      Serial.println("Boiler on");
      digitalWrite(boilerSwitchPin, HIGH);
    }
    else{
      boilerState = 0;
      Serial.println("Boiler off");
      digitalWrite(boilerSwitchPin, LOW);
    }
  }
  else {
  }
}

uint16 previousMillis = 0;

void loop() { 
  // Keeping the client connected.
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
