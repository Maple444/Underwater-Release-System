#include <RTClib.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define SERVICE_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define CHAR_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"

byte servoPin = 2;
Servo servo;
RTC_DS3231 rtc;

volatile bool newInput = false;
char pending[24];
bool armed = false;
DateTime target;

class ServerCB : public BLEServerCallbacks {
  void onDisconnect(BLEServer *s) {
    BLEDevice::startAdvertising();
  }
};

class CharCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    auto v = c->getValue();  
    strncpy(pending, v.c_str(), sizeof(pending) - 1);
    pending[sizeof(pending) - 1] = 0;
    newInput = true;  
  }
};

void delayme(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

void runMotor() {
  Serial.println("Turning on motor...");
  servo.writeMicroseconds(1500);
  delayme(1000);
  servo.writeMicroseconds(1400);
  delayme(1000);
  servo.writeMicroseconds(1500);
  delayme(2000);
  Serial.println("Code is done"); // Creates funtion "runMotor" for running sequence when the date is recived
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  servo.attach(servoPin);
  servo.writeMicroseconds(1500); //sets the output to a neutral pwm signal
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) Serial.println("RTC lost power, set the time!");

  BLEDevice::init("MotorController");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);
  BLECharacteristic *ch = svc->createCharacteristic(CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  ch->setCallbacks(new CharCB());
  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();
  delayme(7000);  // ESC arming time
}
void loop() {
  if (newInput) {
    newInput = false;
    int y, mo, d, h, mi, s;
    if (sscanf(pending, "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) == 6) {
      target = DateTime(y, mo, d, h, mi, s);
      armed = true;
      Serial.printf("Target set: %s\n", pending);
      for (int i = 0; i < 3; i++) {  // XIAO C6 LED is active low; blinking works either way
        digitalWrite(LED_BUILTIN, HIGH);
        delayme(200);
        digitalWrite(LED_BUILTIN, LOW);
        delayme(200);
      }
    } else {
      Serial.println("Bad format, use YYYY-MM-DD HH:MM:SS");
    }
  }

  if (armed && rtc.now().unixtime() >= target.unixtime()) {
    armed = false;  // clear first so it only fires once
    runMotor(); // activates function from top of code
  }
  delayme(20);
}