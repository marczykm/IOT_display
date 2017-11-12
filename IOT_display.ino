#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "SSD1306.h"
#include "SH1106.h"
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <Ticker.h>

Ticker ticker;

SSD1306 display(0x3C, 4, 5);
ADC_MODE(ADC_VCC);

const char* mqtt_server = "marczyk.it";

WiFiClient espClient;
PubSubClient client(espClient);

#define ONBOARD_LED 2
#define RESET_PIN 0
//#define DEBUG

unsigned long previousMillis = 0;
const long interval = 5000;

int lastPercent = 0;
float lastVoltage = 0;
boolean first = true;

float temperature = 0;
boolean rain = false;

void setup() {
  Serial.begin(9600);
  pinMode(ONBOARD_LED, OUTPUT);
#ifdef RESET_PIN
  pinMode(RESET_PIN, INPUT);
#endif
  digitalWrite(ONBOARD_LED, LOW);

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

  WiFiManager wifiManager;

#ifdef RESET_PIN
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("RESET");
    tickerOn();
    configureInfo();

    if (!wifiManager.startConfigPortal()) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }
  tickerOff();
#endif

  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("connected...yeey :)");
  digitalWrite(ONBOARD_LED, HIGH);
  tickerOff();
}

void tick() {
  int state = digitalRead(ONBOARD_LED);
  digitalWrite(ONBOARD_LED, !state);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  tickerOn();
  configureInfo();
}

void configureInfo() {
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Connect to AP named:");
  String AP = " ESP" + String(ESP.getChipId());
  display.drawString(0, 10, AP);
  display.drawString(0, 25, "go to IP address");
  display.drawString(0, 35, " 192.168.4.1");
  display.drawString(0, 50, "configure your home AP");
  display.display();
}

void loop() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void printTemperature() {
  display.setFont(ArialMT_Plain_16);
  char buf[100];
  String temperatureString = String(temperature);
  temperatureString.toCharArray(buf, sizeof temperatureString);
  display.drawString(0, 0, buf);
  display.drawString(45, 0, "'C");
}

void updateTemperature(String temperatureA) {
  temperature = temperatureA.toFloat();
}

void printRain() {
  display.setFont(ArialMT_Plain_16);
  int rainX = 100;
  if (rain)
    display.drawString(rainX, 0, "''''''''");
  else
    display.drawString(rainX, 0, "_*___");
}

void updateRain(String rainA) {
  if (rainA == "1")
    rain = true;
  else
    rain = false;
}

void updateVccPercent() {
  lastPercent = vccToPercent();
  lastVoltage = getVoltage();
}

void printVcc() {
  display.drawString(0, 25, "bat:");
  display.drawString(30, 25, String(lastVoltage));
  display.drawString(60, 25, "V");

#ifdef DEBUG
  display.drawString(75, 25, "(");
  display.drawString(80, 25, String(ESP.getVcc()));
  display.drawString(115, 25, ")");
#endif

  display.drawProgressBar(1, 55, 120, 8, lastPercent);
}

float getVoltage() {
  return (float)ESP.getVcc() / 1024.0;
}

void sendVcc() {
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
  for (int i = 0; i < length; i++) {
    payloadString = payloadString + (char)payload[i];
  }
  Serial.println(payloadString);
  if (String(topic) == "temperature") {
    updateTemperature(payloadString);
  } else if (String(topic) == "rain") {
    updateRain(payloadString);
  } else if (String(topic) == "display") {
    if (payloadString == "0") {
      display.displayOff();
    } else {
      display.displayOn();
    }
  } else if (String(topic) == "reset") {
    ESP.reset();
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    updateVccPercent();
    sendVcc();
  }
  printVcc();
  printTemperature();
  printRain();
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
      client.subscribe("rain");
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
  int full = 2780;

  Serial.print("current vcc: ");
  Serial.println(ESP.getVcc());

  int currentVcc = ESP.getVcc() - empty;
  double currentPercent = 100 * currentVcc / (full - empty);

  return (int)currentPercent;
}

void tickerOn() {
  ticker.attach(0.2, tick);
}

void tickerOff() {
  ticker.detach();
}

