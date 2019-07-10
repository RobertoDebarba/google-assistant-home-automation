#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <DHT.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>

//AWS
#include "sha256.h"
#include "Utils.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PUBSUBCLIENT LIB 
#include <PubSubClient.h>
#define MQTT_MAX_PACKET_SIZE 2048

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

extern "C" {
  #include "user_interface.h"
}

unsigned long previousMillis = 0;

//AWS IOT config, change these:
char wifi_ssid[]       = "Redmi";
char wifi_password[]   = "qwert12345";
char aws_endpoint[]    = "";
char aws_key[]         = "";
char aws_secret[]      = "";
char aws_region[]      = "us-east-1";
const char* aws_topic  = "$aws/things/arduino-thing/shadow/update";
const char* aws_topic_delta  = "$aws/things/arduino-thing/shadow/update/delta";
int port = 443;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

PubSubClient client(awsWSclient);

DHT dht(0, DHT22); //D3

IRsend irsend(4); //D2

boolean isLedOn = false;
boolean isArOn = false;
boolean isTvOn = false;

uint16_t airConditioningOn[73] = {9050, 4400,  690, 498,  690, 498,  668, 1630,  668, 1656,  692, 1606,  690, 500,  666, 522,  668, 522,  690, 1606,  690, 498,  
                        668, 520,  690, 1608,  690, 498,  666, 522,  666, 522,  690, 498,  668, 520,  666, 524,  664, 522,  668, 522,  666, 522,  666, 522,  666, 522,  
                        690, 500,  690, 1608,  690, 498,  666, 522,  690, 498,  690, 1634,  664, 526,  662, 1634,  690, 498,  690, 498,  666, 1630,  690, 498,  666};

uint16_t airConditioningOff[73] = {9026, 4424,  690, 498,  666, 522,  666, 1658,  664, 524,  664, 1634,  690, 498,  690, 498,  690, 498,  690, 1608,  666, 522,  690, 498,  
                        666, 1632,  688, 498,  690, 498,  668, 522,  666, 522,  690, 498,  690, 498,  690, 498,  690, 498,  666, 522,  690, 498,  690, 498,  666, 522,  666, 1632,  
                        690, 500,  666, 522,  690, 498,  666, 1632,  690, 498,  690, 1634,  690, 498,  664, 524,  664, 1634,  690, 498,  690};                        

uint16_t tvTurnOff[135] = {8964, 4426,  634, 1596,  634, 1598,  634, 494,  634, 496,  632, 494,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 494,  634, 1596,  634, 494, 
                        634, 1596,  634, 494,  634, 1598,  634, 500,  628, 494,  634, 494,  632, 494,  634, 494,  634, 1596,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 1596,  
                        634, 1596,  634, 1596,  634, 494,  634, 1596,  634, 1596,  634, 494,  634, 40844,  8962, 4426,  632, 1596,  634, 1596,  634, 494,  634, 494,  634, 494,  634, 496, 
                        632, 494,  634, 1596,  634, 1596,  634, 494,  634, 1598,  634, 494,  634, 1596,  634, 494,  634, 1596,  634, 494,  634, 494,  632, 496,  632, 496,  632, 494,  634, 
                        1596,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 1596,  634, 1596,  634, 1598,  634, 494,  634, 1596,  634, 1596,  634, 494,  634};                        

uint16_t tvTurnOn[135] = {8964, 4426,  634, 1596,  634, 1598,  634, 494,  634, 496,  632, 494,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 494,  634, 1596,  634, 494, 
                        634, 1596,  634, 494,  634, 1598,  634, 500,  628, 494,  634, 494,  632, 494,  634, 494,  634, 1596,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 1596,  
                        634, 1596,  634, 1596,  634, 494,  634, 1596,  634, 1596,  634, 494,  634, 40844,  8962, 4426,  632, 1596,  634, 1596,  634, 494,  634, 494,  634, 494,  634, 496, 
                        632, 494,  634, 1596,  634, 1596,  634, 494,  634, 1598,  634, 494,  634, 1596,  634, 494,  634, 1596,  634, 494,  634, 494,  632, 496,  632, 496,  632, 494,  634, 
                        1596,  634, 494,  634, 494,  634, 1596,  634, 1596,  634, 1596,  634, 1596,  634, 1598,  634, 494,  634, 1596,  634, 1596,  634, 494,  634};   

void sendIr(uint16_t rawData[], int rawSize) {
  irsend.sendRaw(rawData, rawSize, 38);  // Send a raw data capture at 38kHz.
  Serial.println("IR sent");
}

float getTemperature() {
  float t = dht.readTemperature();
}

//# of connections
long connection = 0;

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;

//callback to handle mqtt messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  String payloadStr = String((char*)payload);

  Serial.println("Payload:" + payloadStr);
  
  if (payloadStr.indexOf("\"led\":\"on\"") >= 0 && !isLedOn) {
    digitalWrite(2, LOW);
    isLedOn = true;
  } 
  if (payloadStr.indexOf("\"led\":\"off\"") >= 0 && isLedOn) {
    digitalWrite(2, HIGH);
    isLedOn = false;
  } 
  if (payloadStr.indexOf("\"ar\":\"on\"") >= 0 && !isArOn) {
    sendIr(airConditioningOn, 73);
    sendIr(airConditioningOn, 73);
    isArOn = true;
  }
  if (payloadStr.indexOf("\"ar\":\"off\"") >= 0 && isArOn) {
    sendIr(airConditioningOff, 73);
    sendIr(airConditioningOff, 73);
    isArOn = false;
  }
  if (payloadStr.indexOf("\"tv\":\"on\"") >= 0 && !isTvOn) {
    sendIr(tvTurnOn, 135);
    isTvOn = true;
  }
  if (payloadStr.indexOf("\"tv\":\"off\"") >= 0 && isTvOn) {
    sendIr(tvTurnOff, 135);
    isTvOn = false;
  }
  
}

//connects to websocket layer and mqtt layer
bool connect () {
    if (client.connected()) {    
        client.disconnect ();
    }  
    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");


    //creating random client id
    char* clientID = generateClientID ();
    
    client.setServer(aws_endpoint, port);
    if (client.connect(clientID)) {
      Serial.println("connected");     
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      return false;
    }
    
}

//subscribe to a mqtt topic
void subscribe () {
    client.setCallback(callback);
    client.subscribe(aws_topic_delta);
    //subscript to a topic
    Serial.println("MQTT subscribed");
}

//send a message to a mqtt topic
void sendmessage (String message) {
    char copy[message.length() + 1];
    message.toCharArray(copy, message.length() + 1);
    
    char buf[1000];
    strcpy(buf, copy);   
    int rc = client.publish(aws_topic, buf); 
}

void sendData() {
    long interval = 60000;
  
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      String temp = String(dht.readTemperature());
      String hum = String(dht.readHumidity());

      if (temp == "nan") {
        return;
      }
      
      String message = "{\"state\":{\"desired\":{"
                        "\"temp\": \"" + String(dht.readTemperature()) + "\","
                        "\"hum\": \"" + String(dht.readHumidity()) + "\""
                      "}}}";
      sendmessage (message);
      Serial.println("Data sent");
    }
}

void setup() {
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
  
    wifi_set_sleep_type(NONE_SLEEP_T);
    Serial.begin (115200);
    delay (2000);
    Serial.setDebugOutput(1);

    //fill with ssid and wifi password
    WiFiMulti.addAP(wifi_ssid, wifi_password);
    Serial.println ("connecting to wifi");
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
        Serial.print (".");
    }
    Serial.println ("\nconnected");

    irsend.begin();
    Serial.println("IR initialized");

    //fill AWS parameters    
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);

    if (connect ()){
      subscribe ();
      Serial.println("Subscribed");

      String message = "{\"state\":{\"desired\":{"
                        "\"on\": \"true\""
                      "}}}";
      sendmessage (message);
      Serial.println("Message sent");
    }
}

void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {    
      sendData();
    
      client.loop();
      yield();
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();      
    }
  }
}
