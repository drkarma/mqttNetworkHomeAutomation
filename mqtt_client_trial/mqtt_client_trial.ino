/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/
#include "testSSIDParameters.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


// Update these with values suitable for your network.

const char* ssid = privateSSID;  //this is defined in ssidParameters.h
const char* password = privateWiFiPassword; //this is defined in ssidParameters.h
const char* mqtt_server = "192.168.1.110";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
long lastHeartBeat = 0;
char msg[50];
int value = 0;
int updateInoDelay = 30000; // If there is an update announced then do a 30 sec delay
bool updatingInProgress = false; // This global variable will be set true if we get a update request sent to us
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

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic,"/oam/updateino/yard/circle")==0) {
    // We are requested to do an update
    updatingInProgress = true; // We set this true now
    
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("/yard/circle/pir-1", "Established PIR1");
      client.publish("/yard/circle/pir-2", "Established PIR2");

      // ... and resubscribe
      client.subscribe("/yard/circle/pir-1");
      client.subscribe("/yard/circle/pir-2");
      client.subscribe("/oam/updateino/yard/circle");
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
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  ArduinoOTA.setHostname("yardCircleSensorModule"); // give an name to our module
  ArduinoOTA.begin(); // OTA initialization
  
}

void loop() {
  ArduinoOTA.handle(); 
  int pirOneValue = digitalRead(D5);
  int pirTwoValue = digitalRead(D6);
  if (!client.connected()) {
    reconnect();
  }
  if (updatingInProgress) {
    client.publish("/yard/circle/pir-1", "offline");
    client.publish("/yard/circle/pir-2", "offline");

    delay(updateInoDelay);
    updatingInProgress = false;
    }
  client.loop();
  long now = millis();
 

  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("/sensor/outside/temp", msg);
    if(pirOneValue == 1) {
      client.publish("/yard/circle/pir-1", "1");
    }
    if(pirTwoValue == 2) {
      client.publish("/yard/circle/pir-2", "1");
    }
  }
  if (now - lastHeartBeat > 90000) {
    lastHeartBeat = now;
    client.publish("/oam/heartbeat/sensormodule", "yardCircleSensorModule");
  }
}
