//Include Libraries
//For temprature
#include <OneWire.h>
#include <DallasTemperature.h>

//for humidity
#include "DHTesp.h"

//for air pressure
#include <Wire.h>
#include <Adafruit_BMP085.h>

//for Wifi and MQTT
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


//set up the temprature sensor
#define ONE_WIRE_BUS D4
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
//set up the pressure sensor
Adafruit_BMP085 bmp;

// Network Information
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
//and set up the wifi and mqtt libraries
WiFiClient espClient;
PubSubClient client(espClient);

//set up some globals
bool debug = false; //set to true to output a bunch of debuggin info to serial port

//variables used for the loop timing
int updateTimeMS = 60000; //TODO: Make this configurable via MQTT
//int updateTimeMS = 3000; //Set to 3 seconds for development

long lastMsg = 0;
const char* brdName = "o/bedroom/sensorbrd1";
const char* announcePath = "o/announce";
const char* onlineMsg = "bedroom/sensorbrd1 online";
//callback paths
const char* cbGetConfig = "o/bedroom/sensorbrd1/callback/getconfig";
const char* cbSetInterval = "o/bedroom/sensorbrd1/callback/setinterval";

//sensor strings for MQTT
const char* sensorDHT = "dht22/rh";
const char* sensorDS18 = "ds18b20/c";
const char* sensorBMP = "bmp180/pa";


//set up the humidity sensor
DHTesp dht;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //TODO Figure out if I can use a switch/case statement here. that would be way easier
  if (strcmp(topic, cbGetConfig) == 0) {
    //send back the config as a json object
    Serial.println("cbGetConfig");
    Serial.println(updateTimeMS);
  } else if (strcmp(topic,cbSetInterval) == 0){
    Serial.println("cbSetInterval");
    //change the update interval in MS
    //TODO check that we actually recieved an int
    Serial.print("I Got:[");
    //Serial.print((long) payload);
    Serial.print((char*)payload);
    Serial.println("]");
    updateTimeMS = (long) (char*)payload;
  } else {
    Serial.println("Uncaught callback");
  }
  
  // Switch on the LED if an 1 was received as first character
//  if ((char)payload[0] == '1') {
//    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
//  } else {
//    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (debug) {Serial.print("Attempting MQTT connection...");}
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      if (debug) {Serial.println("connected");}
      // Once connected, publish an announcement...
      client.publish(announcePath, onlineMsg);
      // ... and resubscribe
      //subscribe to the brdCallback mqtt address
      //client.subscribe(brdCallback);
      client.subscribe(cbGetConfig);
      client.subscribe(cbSetInterval);


      //client.subscribe("o/bedroom/sensorbrd1/callback/getconfig");
      //client.subscribe("o/bedroom/sensorbrd1/callback/setinterval");
    } else {
      if (debug) {Serial.print("failed, rc=");}
      if (debug) {Serial.print(client.state());}
      if (debug) {Serial.println(" try again in 5 seconds");}
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void wifiConnect(void) {
  delay(10);
  // We start by connecting to a WiFi network
  if (debug) {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debug) {Serial.print(".");}
  }

  randomSeed(micros());

  if (debug) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

int BMPGetPressure(void) {
  return bmp.readPressure();
}

float DSGetTemp(void) {
  //if (debug) {Serial.print("Requesting temperatures...");}
  sensors.requestTemperatures(); // Send the command to get temperatures
  float temp = sensors.getTempCByIndex(0);
  //if (debug) {Serial.println(sensors.getTempCByIndex(0));}
  return temp;
  
}

float DHGetHumid(void) {
  delay(dht.getMinimumSamplingPeriod());

  float humidity = dht.getHumidity();
  return humidity;
}

void setup() {
  Serial.begin(9600);
  if (debug) {  Serial.println("Debugging Mode ON"); }

  //start up the ds sensor library
  sensors.begin();

  dht.setup(16, DHTesp::DHT22); // Connect DHT sensor to GPIO 16

  //connect to the bmp180
  if (!bmp.begin()) {
    if (debug) {Serial.println("Could not find a valid BMP085 sensor, check wiring!");}
    while (1) {}
  }

  //connect to wifi
  wifiConnect();

  if (debug) {Serial.println("about to initialize mqtt");}
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void sendMQTTint(int valueToSend, const char *sensorName) {
  //this function send the mqtt messages
  //it only works with INT. Will write other functions for float or text
  //TODO consolidate the sendMQTT functions to one function
  
  char msgValue[10]; // might need to make this bigger?

  //convert the int to a char array so I can send it over mqtt
  snprintf (msgValue, 50, "%i", valueToSend); //not sure why the 50. I think it's the length of the char array maybe???

  //concatenate the board name and sensor name to get the MQTT path to post to
  char mqttFullPath[80];
    sprintf(mqttFullPath,"%s/%s",brdName,sensorName); //note that brdName is a global varianble, sensorName was passed to the function
    if (debug) { 
      Serial.print(mqttFullPath); 
      Serial.print(" : ");
      Serial.println(msgValue);
    }
    client.publish(mqttFullPath, msgValue, true);
  
}

void sendMQTTfloat(float valueToSend, const char *sensorName) {
  //this function send the mqtt messages
  //it only works with FLOAT. Will write other functions for float or text
  //TODO consolidate the sendMQTT functions to one function
  
  char msgValue[10]; // might need to make this bigger?

  //convert the int to a char array so I can send it over mqtt
  snprintf (msgValue, 50, "%f", valueToSend); //not sure why the 50. I think it's the length of the char array maybe???

  //concatenate the board name and sensor name to get the MQTT path to post to
  char mqttFullPath[80];
    sprintf(mqttFullPath,"%s/%s",brdName,sensorName); //note that brdName is a global varianble, sensorName was passed to the function
    if (debug) { 
      Serial.print(mqttFullPath); 
      Serial.print(" : ");
      Serial.println(msgValue);
    }
    client.publish(mqttFullPath, msgValue, true);
  
}

void loop() {
  //i think thisis an matt thing
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  //variables
  int pressure = 0;
  char msg[10];
  
  //repeat the loop every updateTimeMS seconds
  long now = millis();
  if (now - lastMsg > updateTimeMS) {
    //do stuff
    //get the sensor readings and send MQTT messages for each
    
    sendMQTTfloat(DSGetTemp(), sensorDS18);

    sendMQTTfloat(DHGetHumid(), sensorDHT);

    sendMQTTint(BMPGetPressure(), sensorBMP);

    //update lastMsg for the loop timer
    lastMsg = now;
  }
}
