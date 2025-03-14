#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <lib/GyverStepper.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include "HX711.h"   

#define DT  23                                                
#define SCK 22 
#define SERVO_PIN 26
#define SSID "****"
#define PASS "********"


HX711 scale;
GStepper<STEPPER4WIRE> stepper(2048, 19, 18, 17, 16);
Servo servoMotor;  
BLEScan* pBLEScan;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

float calibration_factor = 0.0014813;                         
float units;                                                 
float ounces;  

int pos = 0;
int RSSI_THRESHOLD = -40;
int scanTime = 2;
int size;
int timezone;

bool device_found;
bool dir = true;

String dayStamp;
String timeStamp;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    HTTPClient http;
    WiFiClient client;
    void onResult(BLEAdvertisedDevice advertisedDevice) {

    String deviceName = advertisedDevice.getName();
    if (http.begin(client, "http://192.168.0.103:8000/collar/get_collars_by_feeder")){
      http.addHeader("accept", "application/json");
      http.addHeader("Content-Type", "application/json");
      StaticJsonDocument<1> TempDataJSON;
      TempDataJSON["feeder_name"] = "FEEDER_001";
      String TempDataString;
      serializeJson(TempDataJSON, TempDataString);
      int httpResponseCode = http.POST(TempDataString);
      String result = http.getString();
      Serial.println(result);
      if (httpResponseCode == 200){
        StaticJsonDocument<200> data;
        deserializeJson(data, result);
        for(byte i = 0; i < sizeof(data); i++){
          if (deviceName == data[i]) 
                        {
          device_found = true;
          
          Serial.println(advertisedDevice.toString().c_str());
                          break;
                        }
        }
      }
    }
    http.end();
}};


void setup() {
  Serial.begin(115200);
  BLEDevice::init("FEEDER_001");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  stepper.setSpeed(10000);
  stepper.setMaxSpeed(10000);
  stepper.setAcceleration(10000);
  scale.begin(DT, SCK);                                      
  scale.set_scale();                                         
  scale.tare();                                              
  scale.set_scale(calibration_factor); 
}


void loop() {
  byte feedTime[][2] = {{}};
  HTTPClient http;
  WiFiClient client;
  timeClient.forceUpdate();
  if (http.begin(client, "http://192.168.0.103:8000/settings/get_settings_by_feeder")){
    http.addHeader("accept", "application/json");
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<1> TempDataJSON;
    TempDataJSON["feeder_name"] = "FEEDER_001";
    String TempDataString;
    serializeJson(TempDataJSON, TempDataString);
    int httpResponseCode = http.POST(TempDataString);
    String result = http.getString();
    Serial.println(result);
    if (httpResponseCode == 200){
      StaticJsonDocument<200> data;
      deserializeJson(data, result);
      size = data["size"];
      timezone = data["timezone"];
      for(byte i = 0; i < sizeof(data["schedule"]) / 2; i++){
        for(byte j = 0; j < 2; j++){
          feedTime[i][j] = data["schedule"][i][j];
        }
      }
    }
  }
  http.end();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  for (int i = 0; i < sizeof(feedTime) / 2; i++){  
    if ((feedTime[i][0] == get_hour(hour, timezone)) && (feedTime[i][1] == minute)){
      отправить вес остатка на сервер
      while(check_size() < size){
        feed();
      }
      if (http.begin(client, "http://192.168.0.103:8000/activity/feeder_feed_activity")){
        http.addHeader("accept", "application/json");
        http.addHeader("Content-Type", "application/json");
        StaticJsonDocument<1> TempDataJSON;
        TempDataJSON["feeder"] = "FEEDER_001";
        String TempDataString;
        serializeJson(TempDataJSON, TempDataString);
        int httpResponseCode = http.POST(TempDataString);
      }
    }
  } 
  old_pos = pos;
  device_found = false;
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  for (int i = 0; i < foundDevices->getCount(); i++)
  {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    int rssi = device.getRSSI();
    if (rssi > RSSI_THRESHOLD && device_found == true)
      {
        Serial.println("its here");
        pos = 180;
        break;

  }
    else{
    pos = 0;
  }
  }
  Serial.print("pos = ");
  Serial.println(pos);
  servoMotor.write(pos);
  if (old_pos == 180 && pos == 0){
    if (http.begin(client, "http://192.168.0.103:8000/activity/eating_activity")){
      http.addHeader("accept", "application/json");
      http.addHeader("Content-Type", "application/json");
      StaticJsonDocument<1> TempDataJSON;
      TempDataJSON["feeder_name"] = "FEEDER_001";
      TempDataJSON["collar_name"] = collar;
      String TempDataString;
      serializeJson(TempDataJSON, TempDataString);
      int httpResponseCode = http.POST(TempDataString);
      String result = http.getString();
    }
    http.end();
  }
  pBLEScan->clearResults();

}

int check_size(){
  for (int i = 0; i < 10; i ++) {                             
    units = + scale.get_units(), 10;                         
  }
  units = units / 10;                                         
  ounces = units * 0.035274;                                 
  Serial.print(int(ounces));                                     
  Serial.println(" grams"); 
  return int(ounces);
}


void feed() {
  if (!stepper.tick()) {
    dir = !dir;
    stepper.setTarget(dir ? -90 : 90, RELATIVE);
  }
}

int get_hour(int hour, int timezone){
  int hour_in_timezone = hour + timezone;
  if (hour_in_timezone < 24){
    return hour_in_timezone;
  }
  else {
    return hour_in_timezone - 24;
  }
}

