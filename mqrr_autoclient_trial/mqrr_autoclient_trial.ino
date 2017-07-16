#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ssidParameters.h" // To be removed once we can update MQTT server online

//GLOBAL STUFF ON TOP
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
long lastHeartBeat = 0;
char msg[50];
int value = 0;
int updateInoDelay = 30000; // If there is an update announced then do a 30 sec delay
bool updatingInProgress = false; // This global variable will be set true if we get a update request sent to us
const char* mqtt_server = privateMQTTServer;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  ArduinoOTA.setHostname("mqttClientType"); // give an name to our module
  ArduinoOTA.begin(); // OTA initialization
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(180);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
 
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



void loop() {
  // put your main code here, to run repeatedly:
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
    snprintf (msg, 75, "Autoconf hello world #%ld", value);
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
    client.publish("/oam/heartbeat/sensormodule", "AutoConfigWifiManager");
  }
}

