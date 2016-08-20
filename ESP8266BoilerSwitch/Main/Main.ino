/*
  Boiler Switch 0.3

  20 August 2016

  Started using JSON object string to pass data between esp8266 devices and node-red
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "Config.h"         // Contains all the SSID and MQTT config data (such as usernames, passwords, and topics).

int boilerState = 0;
#define boilerSwitchPin D1  // Pin used to control the relay on the WiMos Mini board

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
    if (client.connect(esp8266_client, mqtt_username, mqtt_password)) { //, mqtt_username, mqtt_password)) {
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
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Received something");
  
  // create character buffer with ending null terminator (string)
  for (int i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[length + 1] = '\0';

  StaticJsonBuffer<200> jsonBuffer;
  StaticJsonBuffer<200> jsonBufferSend;
  JsonObject& root = jsonBuffer.parseObject(message_buff);
  JsonObject& rootSend = jsonBufferSend.createObject();
  boilerState = root["value"];
  const char* type = root["type"];
  String typeString = String(type);

  if (typeString == "boilerState"){
    rootSend["type"] = "boilerState";
    Serial.print("Boiler state: ");
    if (boilerState == 1){
      Serial.println("ON");
      digitalWrite(boilerSwitchPin, HIGH);
      rootSend["value"] = 1;
    }
    else{
      Serial.println("OFF");
      digitalWrite(boilerSwitchPin, LOW);
      rootSend["value"] = 0;
    }
    char sendBuffer[200];
    rootSend.printTo(sendBuffer, sizeof(sendBuffer));
    client.publish(mqtt_send_topic, sendBuffer);
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
