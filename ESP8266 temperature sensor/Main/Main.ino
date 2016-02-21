/*
  DS1631_MQTT_Temperature_Sensor_0.2
  21 Feb 2016
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Wire.h>
#include <DS1631.h>
#include "Config.h"         // Contains all the SSID and MQTT config data (such as usernames, passwords, and topics).

// Temperature Sensor
DS1631 Temp1(0);

// Ticker / Timer
Ticker flipper;
int ticker_delay = 60;

// Wifi
WiFiClient espClient;
PubSubClient client(espClient);
char msg[5];

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

void getTemp(){
  Serial.println("Getting Temperature.");

  uint16_t temp = Temp1.readTempOneShotInt();
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
}

void setup() {
  Serial.begin(115200);

  // Setting up wifi
  Serial.println("Setting up wifi.");
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Setting up timer
  Serial.println("Setting up timer.");
  flipper.attach(ticker_delay, getTemp);

  // Setting up DS1631
  Serial.println("Setting up DS1631.");
  Wire.begin(2, 14);
  int config = Temp1.readConfig();
  Serial.print("Config settings before: ");
  Serial.println(config, BIN);

  Temp1.writeConfig(13);
  config = Temp1.readConfig();
  Serial.print("Config settings after: ");
  Serial.println(config, BIN);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
