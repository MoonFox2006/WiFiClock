#define USE_SHT3X

#include <pgmspace.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "MAX7219.h"
#include "Date.h"
#ifdef USE_SHT3X
#include "SHT3x.h"
#endif

const uint32_t WIFI_REPEAT = 30 * 1000; // 30 sec.
const uint32_t NTP_UPDATE = 3600 * 8 * 1000; // 8 hour
const uint32_t NTP_REPEAT = 30 * 1000; // 30 sec.

const uint8_t MORNING_HOUR = 8;
const uint8_t FULL_BRIGHTNESS = 4;
const uint8_t EVENING_HOUR = 22;
const uint8_t HALF_BRIGHTNESS = 0;

const char WIFI_SSID[] PROGMEM = "YOUR_SSID";
const char WIFI_PSWD[] PROGMEM = "YOUR_PSWD";
const char NTP_SERVER[] PROGMEM = "pool.ntp.org";
const int8_t NTP_TZ = 3; // GMT+3

enum : uint8_t { MODE_IDLE, MODE_WIFI, MODE_NTP };

MAX7219<D8, 4> display;
#ifdef USE_SHT3X
SHT3x<> *sht = nullptr;
#endif
volatile uint32_t wifi_next = 0;
uint32_t ntp_time = 0;
uint32_t ntp_last = 0;
uint32_t ntp_next = 0;
volatile uint8_t mode = MODE_IDLE;

/*
static void halt(const __FlashStringHelper *msg) {
  Serial.println(msg);
  Serial.flush();
  display.scroll((PGM_P)msg);
  delay(5000);
  display.end();
  ESP.deepSleep(0);
}
*/

static void wifiOnEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      mode = MODE_IDLE;
      Serial.println(F("WiFi disconnected"));
      if (! ntp_time)
        display.noAnimate();
      wifi_next = millis() + WIFI_REPEAT;
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
      mode = MODE_NTP;
      Serial.println(F("WiFi connected"));
      if (! ntp_time)
        display.noAnimate();
      wifi_next = 0;
      break;
    default:
      break;
  }
}

static void wifiConnect(PGM_P ssid, PGM_P pswd) {
  static const uint8_t PROGRESS[] PROGMEM = {
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x24, 0x48, 0x10, 0x60, 0x00, 0x00,
    0x55, 0x25, 0x4A, 0x12, 0x64, 0x18, 0x60,
  };

  if ((! wifi_next) || ((int32_t)(wifi_next - millis()) <= 0)) {
    char _ssid[strlen_P(ssid) + 1];
    char _pswd[strlen_P(pswd) + 1];

    strcpy_P(_ssid, ssid);
    strcpy_P(_pswd, pswd);
    mode = MODE_WIFI;
    WiFi.begin(_ssid, _pswd);
    Serial.printf_P(PSTR("Connecting to \"%s\"...\n"), _ssid);
    if (! ntp_time) {
      display.printStr(0, 0, PSTR("WiFi"));
      display.animate(display.width() - 7, 0, 7, 8, 4, PROGRESS, 250);
    }
  }
}

static uint32_t ntpTime(PGM_P server, int8_t tz, uint32_t timeout = 1000, uint8_t repeat = 1) {
  const uint16_t LOCAL_PORT = 55123;

  if (WiFi.isConnected()) {
    WiFiUDP udp;

    if (udp.begin(LOCAL_PORT)) {
      char _server[strlen_P(server) + 1];

      strcpy_P(_server, server);
      do {
        uint8_t buffer[48];

        memset(buffer, 0, sizeof(buffer));
        // Initialize values needed to form NTP request
        buffer[0] = 0B11100011; // LI, Version, Mode
        buffer[1] = 0; // Stratum, or type of clock
        buffer[2] = 6; // Polling Interval
        buffer[3] = 0xEC; // Peer Clock Precision
        // 8 bytes of zero for Root Delay & Root Dispersion
        buffer[12] = 49;
        buffer[13] = 0x4E;
        buffer[14] = 49;
        buffer[15] = 52;
        // all NTP fields have been given values, now
        // you can send a packet requesting a timestamp
        if (udp.beginPacket(_server, 123) && (udp.write(buffer, sizeof(buffer)) == sizeof(buffer)) && udp.endPacket()) {
          uint32_t time = millis();
          int cb;

          while ((! (cb = udp.parsePacket())) && (millis() - time < timeout)) {
            delay(1);
          }
          if (cb) {
            time = millis() - time;
            // We've received a packet, read the data from it
            if (udp.read(buffer, sizeof(buffer)) == sizeof(buffer)) { // read the packet into the buffer
              time = (((uint32_t)buffer[40] << 24) | ((uint32_t)buffer[41] << 16) | ((uint32_t)buffer[42] << 8) | buffer[43]) - 2208988800UL;
              time += tz * 3600;
              return time;
            }
          }
        }
      } while (repeat--);
    }
  }
  return 0;
}

static bool isEvening(uint8_t h) {
  if (EVENING_HOUR > MORNING_HOUR) {
    return (h < MORNING_HOUR) || (h >= EVENING_HOUR);
  } else {
    return (h >= EVENING_HOUR) && (h < MORNING_HOUR);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

#ifdef USE_SHT3X
  sht = new SHT3x<>();
  sht->init();
  if (! sht->begin()) {
    delete sht;
    sht = nullptr;
    Serial.println(F("SHT3x not found!"));
  }
#endif

  display.init();
  display.begin(HALF_BRIGHTNESS);
//  display.scroll(PSTR("Hello!"));
  display.scroll(PSTR("\xC7\xE4\xF0\xE0\xE2\xF1\xF2\xE2\xF3\xE9\xF2\xE5!")); // "Здравствуйте!"
  delay(5000);
  display.clear();

#ifdef USE_SHT3X
  if (! sht) {
    display.scroll(PSTR("\xCD\xE5\xF2 SHT3x!")); // "Нет SHT3x!"
    delay(5000);
    display.clear();
  }
#endif

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiOnEvent);
}

void loop() {
  uint32_t t;

  if ((! ntp_time) || ((int32_t)(ntp_next - millis()) <= 0)) {
    if (mode == MODE_IDLE) {
      wifiConnect(WIFI_SSID, WIFI_PSWD);
    } else if (mode == MODE_NTP) {
      t = ntpTime(NTP_SERVER, NTP_TZ);
      if (t) {
        ntp_time = t;
        ntp_last = millis();
        ntp_next = ntp_last + NTP_UPDATE;
        Serial.println(F("NTP updated"));
        if (isEvening((t / 3600) % 24))
          display.setBrightness(HALF_BRIGHTNESS);
        else
          display.setBrightness(FULL_BRIGHTNESS);
      } else {
        ntp_next = millis() + NTP_REPEAT;
        Serial.println(F("NTP fail!"));
      }
//      mode = MODE_IDLE;
      WiFi.disconnect();
    }
  }

  if (ntp_time) {
    t = ntp_time + (millis() - ntp_last) / 1000;
    if (t % 60 < 50) {
      char str[12];
#ifdef USE_SHT3X
      float tp, hm;
#endif
      uint8_t h, m, s;

      parseEpochTime(t, &h, &m, &s);
      if ((s <= 1) && (m == 0)) { // Beginning of hour
        if (h == MORNING_HOUR)
          display.setBrightness(FULL_BRIGHTNESS);
        else if (h == EVENING_HOUR)
          display.setBrightness(HALF_BRIGHTNESS);
      }
#ifdef USE_SHT3X
      tp = hm = NAN;
      if (sht && (((s >= 10) && (s < 20)) || ((s >= 30) && (s < 40)))) {
        sht->measure(&tp, &hm);
      }
      if ((! isnan(tp)) && (! isnan(hm))) {
        sprintf_P(str, PSTR("%0.1f\xB0 %0.1f%%"), tp, hm);
        display.scroll(str);
        delay((10 - s % 10) * 1000);
        display.noScroll();
      } else {
#endif
        sprintf_P(str, PSTR("%02u:%02u"), h, m);
        t = (display.width() - display.strWidth(str)) / 2;
        display.beginUpdate();
        display.clear();
        display.printStr(t, 0, str);
        if (s & 0x01) {
          str[2] = '\0';
          display.drawPattern(t + display.strWidth(str) + display.FONT_GAP, 0, display.charWidth(':'), display.FONT_HEIGHT, (uint8_t)0);
        }
        display.endUpdate();
        delay(1000);
#ifdef USE_SHT3X
      }
#endif
    } else {
      static const char WEEKDAYS[7][3] PROGMEM = {
        "\xCF\xED", "\xC2\xF2", "\xD1\xF0", "\xD7\xF2", "\xCF\xF2", "\xD1\xE1", "\xC2\xF1" // "Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"
      };

      char str[15];
      uint16_t y;
      uint8_t w, d, m;

      parseEpochDate(t, &w, &d, &m, &y);
      sprintf_P(str, PSTR("%S %02u.%02u.%u"), WEEKDAYS[w], d, m, y);
      display.scroll(str);
      delay((60 - t % 60) * 1000);
      display.noScroll();
    }
  }
}