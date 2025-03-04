#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <lib/GyverStepper.h>

GStepper<STEPPER4WIRE> stepper(2048, 19, 18, 17, 16);
#define SERVO_PIN 26 
Servo servoMotor;  
int pos = 0;
String knownBLENames[] = {"OSHEYNIK_001", "OSHEYNIK_002"};
int RSSI_THRESHOLD = -35;
bool device_found;
int scanTime = 2;
BLEScan* pBLEScan;
const char* ssid     = "111";
const char* password = "11111111";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String dayStamp;
String timeStamp;


int feedAmount = 10;
bool dir = true;
int timezone = 8;

const byte feedTime[][2] = {
  {6, 0},       
  {12, 0},
  {17, 0},
  {22, 11},
};


class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {

    String deviceName = advertisedDevice.getName();

      for (int i = 0; i < (sizeof(knownBLENames) / sizeof(knownBLENames[0])); i++)
      {
        if (deviceName == knownBLENames[i].c_str()) 
                        {
          device_found = true;
          
          Serial.println(advertisedDevice.toString().c_str());
                          break;
                        }
    }
}};
void setup() {
  Serial.begin(115200);
  BLEDevice::init("Feeder001");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);
  WiFi.begin(ssid, password);
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
}


void loop() {
  device_found = false;
  timeClient.forceUpdate();
  for (byte i = 0; i < sizeof(feedTime) / 2; i++){  
    if ((feedTime[i][0] == get_hour(timeClient.getHours().toInt(), timezone)) && (feedTime[i][1] == timeClient.getMinutes().toInt())){
        feed();
      }
    } 
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  for (int i = 0; i < foundDevices->getCount(); i++)
  {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    int rssi = device.getRSSI();
    if (rssi > RSSI_THRESHOLD && device_found == true)
      {
        Serial.println("its here");
        pos = 180;

  }
      
    else{
    pos = 0;
  }
  }
  Serial.print("pos ");
  Serial.println(pos);
  servoMotor.write(pos);
  pBLEScan->clearResults();
}

void feed() {

  
  int tmp_fa = feedAmount;
  while (tmp_fa>0){
    delay(1);
    anti_jam();
    tmp_fa--;
  }

  
}

void anti_jam() {
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
