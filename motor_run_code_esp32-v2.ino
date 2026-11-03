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
DateTime t;

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


static bool allDigits(const char *p, int n) {
  for (int i = 0; i < n; i++)
    if (p[i] < '0' || p[i] > '9') return false;
  return true;
}

static int num(const char *p, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) v = v * 10 + (p[i] - '0');
  return v;
}

bool parseTimestamp(const char *s, DateTime &out) {
  // Shape: exactly 19 chars, separators in the right places
  if (strlen(s) != 19) return false;
  if (s[4] != '-' || s[7] != '-' || s[10] != ' ' || s[13] != ':' || s[16] != ':') return false;

  // Every other position must be a digit
  if (!allDigits(s, 4) || !allDigits(s + 5, 2) || !allDigits(s + 8, 2) ||
      !allDigits(s + 11, 2) || !allDigits(s + 14, 2) || !allDigits(s + 17, 2)) return false;

  int y = num(s, 4), mo = num(s + 5, 2), d = num(s + 8, 2);
  int h = num(s + 11, 2), mi = num(s + 14, 2), sec = num(s + 17, 2);

  // Ranges (the DS3231/RTClib covers 2000 to 2099)
  if (y < 2000 || y > 2099) return false;
  if (mo < 1 || mo > 12) return false;
  if (h > 23 || mi > 59 || sec > 59) return false;

  // Days in month, including leap years
  static const uint8_t dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = dim[mo - 1];
  if (mo == 2 && (y % 4 == 0)) maxDay = 29;   // fine for 2000 to 2099
  if (d < 1 || d > maxDay) return false;

  out = DateTime(y, mo, d, h, mi, sec);
  return true;
}

void delayme(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

void runMotor() {
  Serial.println("Turning on motor...");
  servo.writeMicroseconds(1500);
  vTaskDelay(pdMS_TO_TICKS(1000));
  servo.writeMicroseconds(1400);
  vTaskDelay(pdMS_TO_TICKS(1000));
  servo.writeMicroseconds(1500);
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.println("Code is done");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  servo.attach(servoPin);
  servo.writeMicroseconds(1500);
  Wire.begin();


  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) 
  {
    Serial.println("RTC lost power, check wiring!");
   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  }

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

// visual indication we are waiting for startup time
    Serial.println("Warming up");
   for ( int d=0;d<50;d++)
 {
        digitalWrite(LED_BUILTIN, HIGH);
        delayme(200);
        digitalWrite(LED_BUILTIN, LOW);
        delayme(200);
 }

   Serial.println("Ready"); //solid led no blinking when intialized
}

void loop() {
if (newInput) {
  newInput = false;

  // Strip trailing CR/LF/space, since some BLE apps add a newline
  size_t n = strlen(pending);
  while (n > 0 && (pending[n - 1] == '\n' || pending[n - 1] == '\r' || pending[n - 1] == ' '))
    pending[--n] = 0;


  if (parseTimestamp(pending, t)) {
    target = t;
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
   
   if (t.unixtime() <= rtc.now().unixtime()) {
    Serial.println("Time is in the past"); 
    armed = false;
    }

  if (armed && rtc.now().unixtime() >= target.unixtime()) {
    armed = false;  // clear first so it only fires once
    runMotor();
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}