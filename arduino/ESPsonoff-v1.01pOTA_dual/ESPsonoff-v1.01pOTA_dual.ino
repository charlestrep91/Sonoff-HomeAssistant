/*

  Copyright (c) 2017 @KmanOz
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  ==============================================================================
  Changes in v1.01pOTA
  
    - Relay state now stored in EEPROM and will power up with last relay state
    - OTA Firmware Upgradable
  ==============================================================================

  **** USE THIS Firmware for: Sonoff Dual ****

*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Ticker.h>

#define DEBUG
#define BUTTON          0
#define LED             13                                   // WIFI LED
#define MQTT_SERVER     "192.168.0.101"                      // mqtt server
#define MQTT_PORT       1883                                 // mqtt port
#define MQTT_USER       ""                               // mqtt user
#define MQTT_PASS       ""                               // mqtt password

#define WIFI_SSID       "Sarah&Charles"                           // wifi ssid
#define WIFI_PASS       "oiseauwifi"                           // wifi password
#define DEBUG_ESP_OTA 1

#define VERSION    "\n\n----------------  Sonoff Powerpoint v1.01pOTA  -----------------"

bool rememberRelayState = true;                              // If 'true' remembers the state of the relay before power loss.
bool OTAupdate = false;                                      // (Do not Change)
bool sendStatus = false;                                     // (Do not Change)
bool requestRestart = false;                                 // (Do not Change)

int kUpdFreq = 1;                                            // Update frequency in Mintes to check for mqtt connection
int kRetries = 20;                                           // WiFi retry count. Increase if not connecting to router.
int lastRelay1State;
int lastRelay2State;
String MQTT_TOPIC1;
String MQTT_TOPIC2;
String MQTT_CLIENT;

unsigned long TTasks;                                        // (Do not Change)
unsigned long count = 0;                                     // (Do not Change)

extern "C" { 
  #include "user_interface.h" 
}

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient, MQTT_SERVER, MQTT_PORT);
Ticker btn_timer;

void callback(const MQTT::Publish& pub) {
  int *relay;
  if(pub.topic() == MQTT_TOPIC1)
    relay = &lastRelay1State;
  else if(pub.topic() == MQTT_TOPIC2)
    relay = &lastRelay2State;
  else
    return;
  
  if (pub.payload_string() == "on") {
    *relay = 1;
  }
  else if (pub.payload_string() == "off") {
    *relay = 0;
  }
  else if (pub.payload_string() == "reset") {
    requestRestart = true;
    return;
  }
  else
  {
    return;
  }
  setRelays(lastRelay1State, lastRelay2State);
  sendStatus = true;
}

void setup() {
  MQTT_TOPIC1 = "home/sonoff/" + String(ESP.getChipId(), HEX) + "/1/";
  MQTT_TOPIC2 = "home/sonoff/" + String(ESP.getChipId(), HEX) + "/2/";
  MQTT_CLIENT = "Sonoff_" + String(ESP.getChipId(), HEX);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  Serial.begin(19200);
  EEPROM.begin(8);
  lastRelay1State = EEPROM.read(0);
  lastRelay2State = EEPROM.read(1);
  if (rememberRelayState && (lastRelay1State | lastRelay2State)) 
  {
    setRelays(lastRelay1State, lastRelay2State);
  }
  btn_timer.attach(0.05, button);
  mqttClient.set_callback(callback);
#ifdef DEBUG
  Serial.println(VERSION);
  Serial.print("\nUnit ID: ");
  Serial.print("esp8266-");
  Serial.print(ESP.getChipId(), HEX);
  Serial.print("\nMQTT topics: \n");
  Serial.print(MQTT_TOPIC1);
  Serial.print('\n');
  Serial.print(MQTT_TOPIC2);
  Serial.print("\nConnecting to "); Serial.print(WIFI_SSID); Serial.print(" Wifi");
#endif
  setupWIFI();
  setupOTA();
}

void setupWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while ((WiFi.status() != WL_CONNECTED) && kRetries --) {
    blinkLED(LED, 950, 50, 1);
#ifdef DEBUG
    Serial.print(" .");
#endif
  }
  if (WiFi.status() == WL_CONNECTED) {  
#ifdef DEBUG
    Serial.println(" DONE");
    Serial.print("IP Address is: "); Serial.println(WiFi.localIP());
    Serial.print("Connecting to ");Serial.print(MQTT_SERVER);Serial.print(" Broker ");
#endif
    delay(500);
    while (!mqttClient.connect(MQTT::Connect(MQTT_CLIENT).set_keepalive(90).set_auth(MQTT_USER, MQTT_PASS)) && kRetries --) {
#ifdef DEBUG
      Serial.print(" .");
#endif
      blinkLED(LED, 50, 950, 1);
    }
    if(mqttClient.connected()) {
#ifdef DEBUG
      Serial.println(" DONE");
      Serial.println("\n----------------------------  Logs  ----------------------------");
      Serial.println();
#endif
      mqttClient.subscribe(MQTT_TOPIC1);
      mqttClient.subscribe(MQTT_TOPIC2);
      blinkLED(LED, 40, 40, 8);
    }
    else {
#ifdef DEBUG
      Serial.println(" FAILED!");
      Serial.println("\n----------------------------------------------------------------");
      Serial.println();
#endif
    }
  }
  else {
#ifdef DEBUG
    Serial.println(" WiFi FAILED!");
    Serial.println("\n----------------------------------------------------------------");
    Serial.println();
#endif
  }
}

void setupOTA() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([]() {
    OTAupdate = true;
    blinkLED(LED, 400, 400, 2);
    digitalWrite(LED, HIGH);
    Serial.println("OTA Update Initiated . . .");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Ended . . .s");
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    digitalWrite(LED, LOW);
    delay(5);
    digitalWrite(LED, HIGH);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    blinkLED(LED, 40, 40, 2);
    OTAupdate = false;
    Serial.printf("OTA Error [%u] ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(". . . . . . . . . . . . . . . Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println(". . . . . . . . . . . . . . . Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println(". . . . . . . . . . . . . . . Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println(". . . . . . . . . . . . . . . Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println(". . . . . . . . . . . . . . . End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

void loop() {
  ArduinoOTA.handle();
  if (OTAupdate == false) { 
    mqttClient.loop();
    timedTasks();
    checkStatus();
  }
}

void blinkLED(int pin, int durationOn, int durationOff, int n) {
  for(int i=0; i<n; i++)  {  
    digitalWrite(pin, HIGH);        
    delay(durationOn);
    digitalWrite(pin, LOW);
    delay(durationOff);
  }
}

void button() {
  if (!digitalRead(BUTTON)) {
    count++;
  } 
  else {
    if (count > 1 && count <= 40)
    {
      if(lastRelay1State == lastRelay2State)
      {
        lastRelay1State = !lastRelay1State;
        lastRelay2State = !lastRelay2State;
      }
      else
      {
        lastRelay1State = 0;
        lastRelay2State = 0;
      }
      setRelays(lastRelay1State, lastRelay2State);
      sendStatus = true;
    }
    else if (count >40){
      Serial.println("\n\nSonoff Rebooting . . . . . . . . Please Wait"); 
      requestRestart = true;
    } 
    count=0;
  }
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED)  {
    if (mqttClient.connected()) {
      Serial.println("mqtt broker connection . . . . . . . . . . OK");
    } 
    else {
      Serial.println("mqtt broker connection . . . . . . . . . . LOST");
      requestRestart = true;
    }
  }
  else { 
    Serial.println("WiFi connection . . . . . . . . . . LOST");
    requestRestart = true;
  }
}

void checkStatus() {
  if (sendStatus)
  {
    if (rememberRelayState) 
    {
      EEPROM.write(0, lastRelay1State);
      EEPROM.write(1, lastRelay2State);
      EEPROM.commit();
    }
    if(lastRelay1State)
    {
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC1 + "stat/", "on").set_retain().set_qos(1));
      Serial.println("Relay 1 . . . . . . . . . . . . . . . . . . ON");
    }
    else
    {
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC1 + "stat/", "off").set_retain().set_qos(1));
      Serial.println("Relay 1 . . . . . . . . . . . . . . . . . . OFF");
    }
    if(lastRelay2State)
    {
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC2 + "stat/", "on").set_retain().set_qos(1));
      Serial.println("Relay 2 . . . . . . . . . . . . . . . . . . ON");
    }
    else
    {
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC2 + "stat/", "off").set_retain().set_qos(1));
      Serial.println("Relay 2 . . . . . . . . . . . . . . . . . . OFF");
    }
    sendStatus = false;
  }
  if (requestRestart)
  {
    blinkLED(LED, 400, 400, 4);
    ESP.restart();
  }
}

void timedTasks() {
  if ((millis() > TTasks + (kUpdFreq*60000)) || (millis() < TTasks)) { 
    TTasks = millis();
    checkConnection();
  }
}

void setRelays(bool relay1, bool relay2)
{
  byte b = 0;
  if (relay1) b++;
  if (relay2) b += 2;
  Serial.write(0xA0);
  Serial.write(0x04);
  Serial.write(b);
  Serial.write(0xA1);
  Serial.flush();
//  if (relay1) client.publish((MQTT_PUBLISH + "/relay1").c_str(), "1");
//  else client.publish((MQTT_PUBLISH + "/relay1").c_str(), "0");
//
//  if (relay2) client.publish((MQTT_PUBLISH + "/relay2").c_str(), "1");
//  else client.publish((MQTT_PUBLISH + "/relay2").c_str(), "0");
}
