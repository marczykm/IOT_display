#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "SSD1306.h"
#include "SH1106.h"

SSD1306 display(0x3C, 4, 5);
ADC_MODE(ADC_VCC);

const char* ssid = "TP-LINK";
const char* password = "dupadupa";
const char* mqtt_server = "marczyk.it";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousMillis = 0;
const long interval = 5000;

int lastPercent = 0;
float lastVoltage = 0;
boolean first = true;

void setup()   {                
  Serial.begin(9600);
  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Temperature\nReader");
  display.drawHorizontalLine(0, 40, 168);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 45, "Marcin Marczyk");
  display.display();
  delay(3000);
  display.clear();
  display.setFont(ArialMT_Plain_16);
  
  connectToWifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void connectToWifi(){
  display.clear();
  display.drawString(0,0,"Connecting to\nWiFi");
  display.display();
  
  WiFi.begin(ssid, password);
  Serial.println();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  display.clear();
  display.drawString(0, 0, "WiFi connected\n");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.display();
}

void printTemperature(String temperature){
  display.drawString(0,0,temperature);
  display.drawString(45,0,"'C");
}

void updateVccPercent() {
  lastPercent = vccToPercent();
  lastVoltage = getVoltage();
}

void printVcc(){
  display.drawString(0,25,"bat:");
  display.drawString(30,25, String(lastVoltage));
  display.drawString(60,25, "V");
  display.drawString(75,25, "(");
  display.drawString(80,25, String(ESP.getVcc()));
  display.drawString(115,25, ")");
  display.drawProgressBar(1, 55, 120, 8, lastPercent);
}

float getVoltage(){
  return (float)ESP.getVcc()/1024.0;
}

void sendVcc(){
  char buf[100];
  int percent = vccToPercent();
  String percentString = String(percent);
  percentString.toCharArray(buf, sizeof percentString);
  client.publish("vcc", buf);
}

void callback(char* topic, byte* payload, unsigned int length) {
  display.clear();
  unsigned long currentMillis = millis();
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String payloadString = "";
  for (int i=0;i<length;i++) {
    payloadString = payloadString+(char)payload[i];
  }
  Serial.println(payloadString);
  if (String(topic) == "temperature"){
    printTemperature(payloadString);
  } else if (String(topic) == "display") {
    if (payloadString == "0"){
      display.displayOff();
    } else {
      display.displayOn();
    }
  } else if (String(topic) == "reset") {
    ESP.reset();
  }
  
  if (currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;
    updateVccPercent();
    sendVcc();
  }
  printVcc();
  display.display();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientId = String("esp" + String(ESP.getChipId()));
    char buf[100];
    clientId.toCharArray(buf, sizeof clientId);
    Serial.print("client id: ");
    Serial.println(buf);
    if (client.connect(buf)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // ... and resubscribe
      client.subscribe("temperature");
      client.subscribe("display");
      client.subscribe("reset");
      if (first)
        client.publish("display", "1");
      first = false;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 second");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
}

int vccToPercent() {
  int empty = 2110;
  int full = 2900;

  Serial.print("current vcc: ");
  Serial.println(ESP.getVcc());

  int currentVcc = ESP.getVcc() - empty;
  double currentPercent = 100 * currentVcc / (full - empty);

  return (int)currentPercent;
}


