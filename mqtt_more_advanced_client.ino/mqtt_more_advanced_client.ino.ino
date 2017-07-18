#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ArduinoOTA.h>           // Needed for Online uploading

#include "Adafruit_Sensor.h"
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//Global stuff
WiFiClient espClient;
PubSubClient client(espClient);
///DHT DEFINES
#define DHTPIN            16         // Pin which is connected to the DHT sensor. Corresponding to D8
// Uncomment the type of sensor in use:
#define DHTTYPE           DHT11     // DHT 11 
//#define DHTTYPE           DHT22     // DHT 22 (AM2302)
//#define DHTTYPE           DHT21     // DHT 21 (AM2301)
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;
String tmp_temp; //see last code block below use these to convert the float that you get back from DHT to a string =str
String tmp_hum;
char temp[50]; // In order to send over MQTT
char hum[50]; // In order to send over MQTT


///= My MQTT stuff
long lastMsg = 0;
long lastHeartBeat = 0;
char msg[50];
int value = 0;
int updateInoDelay = 30000; // If there is an update announced then do a 30 sec delay
bool updatingInProgress = false; // This global variable will be set true if we get a update request sent to us
String myMACaddress;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.1.110";
char mqtt_port[6] = "1883";
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//**************************************
// setup function
//**************************************
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  //Be capable to do remote updates for the esp8266 based board
  ArduinoOTA.setHostname("mqttClientType"); // give an name to our module
  ArduinoOTA.begin(); // OTA initialization
  Serial.println("OTA Enabled...");

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  //At this point we have either failed to mount the FS, failed to read the config parameters or successfully read it.
  Serial.println("Status of parameters");
  Serial.print("MQTT server: " + String(mqtt_server) + "\n");
  Serial.print("MQTT port: " + String(mqtt_port) + "\n");
  Serial.print("Blynk token: " + String(blynk_token) + "\n");
  


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  myMACaddress = WiFi.macAddress();
  myMACaddress.replace(":", "");
  Serial.println(myMACaddress);

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //Setup connection to MQTT server
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);

  //Our pinconfiguration
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);

  //At this point we have either failed to mount the FS, failed to read the config parameters or successfully read it.
  Serial.println("Status of parameters");
  Serial.print("MQTT server: " + String(mqtt_server) + "\n");
  Serial.print("MQTT port: " + String(mqtt_port) + "\n");
  Serial.print("Blynk token: " + String(blynk_token) + "\n");

  //Now activate the DHT sensor
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
   Serial.println("------------------------------------");
  Serial.println("Temperature");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");  
  Serial.println("------------------------------------");
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Humidity");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");  
  Serial.println("------------------------------------");
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;
}

//**************************************
// callback function
//**************************************
void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic,"/oam/updateino/yard/circle")==0) {
    // We are requested to do an update
    updatingInProgress = true; // We set this true now
    
  }
  // are we getting a request to reset wifi? (Taking a module out of testing mode and preparing for other network)
  
  if ((String(topic) == "/oam/resetwifi/"+ myMACaddress)) {
    // We got the request
    if ((String(topic).indexOf(myMACaddress)) != -1){
      client.publish("/oam/confirm/", "CONFIRMED MAC"); 
      WiFiManager wifiManager;
      wifiManager.resetSettings();
    }
     
    
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
//**************************************
// reconnect function
//**************************************
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
      client.subscribe("/oam/resetwifi/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


//**************************************
// loop function
//**************************************
void loop() {
  //Make us remotely upgradeable
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
     // delay(delayMS);
  // Get temperature event and print its value.
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    tmp_temp = String(event.temperature);
    tmp_temp.toCharArray(temp, tmp_temp.length() + 1); //packaging up the data to publish to mqtt whoa...
    Serial.print("Temperature: ");
    Serial.print(tmp_temp);
    Serial.println(" *C");
    client.publish("/yard/circle/temp", temp);
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    tmp_hum = String(event.relative_humidity);
    tmp_hum.toCharArray(hum, tmp_hum.length() + 1); //packaging up the data to publish to mqtt whoa...
    Serial.print("Humidity: ");
    Serial.print(tmp_hum);
    Serial.println("%");
    client.publish("/yard/circle/humidity", hum);
  }
  }
  if (now - lastHeartBeat > 90000) {
    lastHeartBeat = now;
    client.publish("/oam/heartbeat/sensormodule", "AutoConfigWifiManager");
  }
}
