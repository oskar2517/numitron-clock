#include "RTClib.h"
#include <WiFi.h>
#include "time.h"
#include "secrets.h"

#define TUBES_COUNT 4

#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE "CET-1CEST,M3.5.0/2,M10.5.0/3"

#define PIN_SERIAL 0   
#define PIN_LATCH 2  
#define PIN_SHIFT 1  
#define PIN_RESET 6

#define DECIMAL_POINT(d) ((d) | 0b00000001)

RTC_DS1307 rtc;

uint8_t segments[] = {
  0b11111100, // 0
  0b01100000, // 1
  0b11011010, // 2
  0b11110010, // 3
  0b01100110, // 4
  0b10110110, // 5
  0b10111110, // 6
  0b11100000, // 7
  0b11111110, // 8
  0b11110110  // 9
};

void pulse(int pin) {
  digitalWrite(pin, LOW);
  delay(5);
  digitalWrite(pin, HIGH);
}

uint8_t mask(uint8_t segments) {
  uint8_t a = ((segments >> 7) & 1) << 2;
  uint8_t b = ((segments >> 6) & 1) << 1;
  uint8_t c = ((segments >> 5) & 1) << 4;
  uint8_t d = ((segments >> 4) & 1) << 5;
  uint8_t e = ((segments >> 3) & 1) << 6;
  uint8_t f = ((segments >> 2) & 1) << 0;
  uint8_t g = ((segments >> 1) & 1) << 3;
  uint8_t h = ((segments >> 0) & 1) << 7;

  return a | b | c | d | e | f | g | h;
}

void displayDigit(int digit, bool decimalPoint) {
  uint8_t digitSegments = mask(decimalPoint ? DECIMAL_POINT(segments[digit]) : segments[digit]);

  for (int i = 0; i < 8; i++) {
    uint8_t bit = (digitSegments >> i) & 1;
    digitalWrite(PIN_SERIAL, bit);
    pulse(PIN_SHIFT);
    digitalWrite(PIN_SERIAL, LOW);
  }
}

void displayNumber(int number, bool decimalPoint) {
  for (int i = 0; i < TUBES_COUNT; i++) {
    int digit = number % 10; 
    displayDigit(digit, decimalPoint);
    number /= 10; 
  }

  pulse(PIN_LATCH);
}

bool connectWifi(void) {
  Serial.print("Connecting to WIFI with SSID ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempt++;
    if (attempt > 10) {
      Serial.println("");
      Serial.println("Failed to connect to WIFI.");
      return false;
    }
  }
  Serial.println("");

  Serial.println("WiFi connected.");

  return true;
}

void disconnectWifi(void) {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WIFI disconnected.");
}

void fetchTimeFromNtp(void) {
  if (!connectWifi()) return;

  configTzTime(TIMEZONE, NTP_SERVER);

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to local obtain time.");
    return;
  }

  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;
  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  int second = timeinfo.tm_sec;

  rtc.adjust(DateTime(year, month, day, hour, minute, second));
  Serial.print("Time adjusted to ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  disconnectWifi();
}

void setup(void) {
  Serial.begin(9600);

  pinMode(PIN_SERIAL, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_SHIFT, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);

  pulse(PIN_RESET);

  while (!rtc.begin()) {
    delay(10);
  }

  fetchTimeFromNtp();

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop(void) {
  static int counter = 0;
  counter++;
  if (counter % (2 * 60 * 30) == 0) {
    fetchTimeFromNtp();
  }

  static bool decimalPoint = false;

  DateTime time = rtc.now();

  int hhmm = time.hour() * 100 + time.minute();

  displayNumber(hhmm, decimalPoint);
  decimalPoint = !decimalPoint;

  delay(500);
}
