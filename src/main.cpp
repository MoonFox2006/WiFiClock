#define USE_SERIAL
#define USE_LLMNR
#define USE_SHT3X

#define LED_PIN   2
#define LED_LEVEL LOW

#include <pgmspace.h>
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#ifdef USE_LLMNR
#include <ESP8266LLMNR.h>
#endif
#include <ESPAsyncWebServer.h>
#include "Parameters.h"
#include "Logger.h"
#ifdef LED_PIN
#include "Leds.h"
#endif
#include "HtmlHelper.h"
#include "Ntp.h"
#include "ActionQueue.h"
#include "MAX7219.h"
#include "Date.h"
#ifdef USE_SHT3X
#include "SHT3x.h"
#endif

#ifdef LED_PIN
#define LED_WIFI  LED_4HZ
#ifdef USE_MQTT
#define LED_MQTT  LED_2HZ
#endif
#define LED_IDLE  LED_05HZ

#define LED_CP0   LED_4HZ
#define LED_CP1   LED_2HZ
#endif

#define CP_SSID "ESP_"
#define CP_PSWD "1029384756"

//#define DEF_WIFI_SSID   "YOUR_SSID"
//#define DEF_WIFI_PSWD   "YOUR_PSWD"
#define DEF_ADM_NAME    "admin"
#define DEF_ADM_PSWD    "12345678"
#define DEF_LLMNR_NAME  "WiFiClock"
#define DEF_NTP_SERVER  "pool.ntp.org"
//#define DEF_NTP_TZ      3
#define DEF_NTP_INTERVAL  (3600 * 4)

const uint8_t RST_CP = 3; // Reboot count to launch captive portal
const uint8_t RST_RESET = 5; // Reboot count to clear configuration

static const char PARAM_WIFI_SSID[] PROGMEM = "wifi_ssid";
static const char PARAM_WIFI_PSWD[] PROGMEM = "wifi_pswd";
static const char PARAM_ADM_NAME[] PROGMEM = "adm_name";
static const char PARAM_ADM_PSWD[] PROGMEM = "adm_pswd";
#ifdef USE_LLMNR
static const char PARAM_LLMNR_NAME[] PROGMEM = "llmnr_name";
#endif
static const char PARAM_NTP_SERVER[] PROGMEM = "ntp_serv";
static const char PARAM_NTP_TZ[] PROGMEM = "ntp_tz";
static const char PARAM_NTP_INTERVAL[] PROGMEM = "ntp_inter";
static const char PARAM_GREETINGS[] PROGMEM = "greetings";
static const char PARAM_MORNING_HOUR[] PROGMEM = "morning_hour";
static const char PARAM_MORNING_BRIGHT[] PROGMEM = "morning_bright";
static const char PARAM_EVENING_HOUR[] PROGMEM = "evening_hour";
static const char PARAM_EVENING_BRIGHT[] PROGMEM = "evening_bright";

static const char URL_ROOT[] PROGMEM = "/";
static const char URL_RESET[] PROGMEM = "/reset";
static const char URL_RESTART[] PROGMEM = "/restart";
static const char URL_OTA[] PROGMEM = "/ota";
static const char URL_WIFI[] PROGMEM = "/wifi";
static const char URL_NTP[] PROGMEM = "/ntp";
static const char URL_LOG[] PROGMEM = "/log";

const uint8_t TEXT_SIZE = 16;

struct __attribute__((__packed__)) config_t {
  char wifi_ssid[32 + 1];
  char wifi_pswd[32 + 1];
  char adm_name[32 + 1];
  char adm_pswd[32 + 1];
#ifdef USE_LLMNR
  char llmnr_name[32 + 1];
#endif
  char ntp_server[32 + 1];
  int8_t ntp_tz;
  uint16_t ntp_interval; // in sec.
  char greetings[15 + 1];
  uint8_t morning_hour;
  uint8_t morning_bright;
  uint8_t evening_hour;
  uint8_t evening_bright;
};

Parameters<config_t> config;
#ifdef USE_SERIAL
Logger<> logger(&Serial);
#else
Logger<> logger();
#endif
#ifdef LED_PIN
Leds<1> led;
#endif
Ticker wifiTimer;
AsyncWebServer http(80);
#ifdef USE_SHT3X
ActionQueue<2> actions;
#else
ActionQueue<1> actions;
#endif
MAX7219<D8, 4> display;
#ifdef USE_SHT3X
SHT3x<> *sht = nullptr;
float temp = NAN;
float hum = NAN;
#endif
volatile bool restarting = false;

static void halt(const __FlashStringHelper *msg = nullptr) {
#ifdef LED_PIN
  led.setMode(0, led.LED_OFF);
#endif
#ifdef USE_SERIAL
  if (msg)
    Serial.println(msg);
  Serial.flush();
#endif
  ESP.deepSleep(0);
}

static void restart(const __FlashStringHelper *msg = nullptr) {
#ifdef LED_PIN
  led.setMode(0, led.LED_OFF);
#endif
#ifdef USE_SERIAL
  if (msg)
    Serial.println(msg);
  Serial.flush();
#endif
  ESP.restart();
}

static void strlcpy_P(char *dest, PGM_P src, size_t size) {
  strncpy_P(dest, src, size - 1);
  dest[size - 1] = '\0';
}

static const char *strOrNull(const char *str) {
  if (str && *str)
    return str;
  return nullptr;
}

static void wifiConnect() {
  static const uint8_t PROGRESS[] PROGMEM = {
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x24, 0x48, 0x10, 0x60, 0x00, 0x00,
    0x55, 0x25, 0x4A, 0x12, 0x64, 0x18, 0x60,
  };

  WiFi.begin(config->wifi_ssid, strOrNull(config->wifi_pswd));
  logger.printf_P(PSTR("Connecting to \"%s\"...\n"), config->wifi_ssid);
#ifdef LED_PIN
  led.setMode(0, led.LED_WIFI);
#endif
  if (! ntpTime()) {
    display.beginUpdate();
    display.clear();
    display.printStr(0, 0, PSTR("WiFi"));
    display.endUpdate();
    display.animate(display.width() - 7, 0, 7, 8, 4, PROGRESS, 250);
  }
}

static void wifiOnEvent(WiFiEvent_t event) {
  static const uint8_t CLOCK[] PROGMEM = {
    0x1C, 0x22, 0x41, 0x4F, 0x49, 0x22, 0x1C
  };

  switch (event) {
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      if (! ntpTime()) {
        display.noAnimate();
        display.clear();
      }
      logger.println(F("WiFi disconnected"));
      if (! restarting)
        wifiTimer.once_ms(5000, wifiConnect);
      http.end();
#ifdef LED_PIN
      led.setMode(0, led.LED_OFF);
#endif
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
      if (! ntpTime()) {
        display.noAnimate();
        display.beginUpdate();
        display.clear();
        display.printStr(0, 0, PSTR("NTP"));
        display.drawPattern(display.width() - 7, 0, 7, 8, CLOCK);
        display.endUpdate();
      }
      wifiTimer.detach();
      logger.print(F("WiFi connected (IP "));
      logger.print(WiFi.localIP());
      logger.println(')');
      http.begin();
#ifdef LED_PIN
      led.setMode(0, led.LED_IDLE);
#endif
      break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      logger.println(F("New AP client connected"));
#ifdef LED_PIN
      led.setMode(0, led.LED_CP1);
#endif
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      logger.println(F("AP client disconnected"));
#ifdef LED_PIN
      if (! WiFi.softAPgetStationNum())
        led.setMode(0, led.LED_CP0);
#endif
      break;
    default:
      break;
  }
}

static size_t encodeString(Print *out, const char *str, uint16_t len = 0) {
  size_t result = 0;

  while (*str) {
    if (*str == '\'')
      result += out->print(F("&apos;"));
    else if (*str == '"')
      result += out->print(F("&quot;"));
    else if (*str == '<')
      result += out->print(F("&lt;"));
    else if (*str == '>')
      result += out->print(F("&gt;"));
    else if (*str == '&')
      result += out->print(F("&amp;"));
    else
      result += out->print(*str);
    if (len) {
      if (! --len)
        break;
    }
    ++str;
  }
  return result;
}

static bool isEvening(uint8_t h) {
  if (config->evening_hour > config->morning_hour) {
    return (h < config->morning_hour) || (h >= config->evening_hour);
  } else {
    return (h >= config->evening_hour) && (h < config->morning_hour);
  }
}

static uint32_t ntpUpdating() {
  if (*config->ntp_server) {
    if (ntpUpdate(config->ntp_server, config->ntp_tz)) {
      logger.println(F("NTP update successful"));
      if (isEvening((ntpTime() / 3600) % 24))
        display.setBrightness(config->evening_bright);
      else
        display.setBrightness(config->morning_bright);
      return config->ntp_interval * 1000;
    } else
      return 5000; // 5 sec. to retry
  } else // Remove action
    return 0;
}

#ifdef USE_SHT3X
static uint32_t shtUpdating() {
  if (sht) {
    if (! sht->measure(&temp, &hum)) {
      logger.println(F("SHT3x read error!"));
    }
    return 2000; // 2 sec.
  } else // Remove action
    return 0;
}
#endif

static bool webAuthorize(AsyncWebServerRequest *request) {
  if ((WiFi.getMode() == WIFI_STA) && *config->adm_name && *config->adm_pswd) {
    if (! request->authenticate(config->adm_name, config->adm_pswd)) {
      request->requestAuthentication();
      return false;
    }
  }
  return true;
}

static void webNotFound(AsyncWebServerRequest *request) {
  if ((WiFi.getMode() == WIFI_AP) && (! request->host().equals(WiFi.softAPIP().toString()))) { // Captive portal
    request->redirect(String(F("http://")) + WiFi.softAPIP().toString());
  } else {
    request->send(404);
  }
}

static void webRoot(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//  response->setCode(200);
  response->print(FPSTR(HTML_PAGE_START));
  response->print(F("WiFi Clock"));
  response->print(FPSTR(HTML_PAGE_CONT));
  response->print(FPSTR(HTML_STYLE_START));
  response->print(F("body{background-color:#eee}\n"
    "a{text-decoration:none;color:black;border:1px solid black;border-radius:10px 25px;padding:8px 16px}\n"));
  response->print(FPSTR(HTML_STYLE_END));
  response->print(FPSTR(HTML_BODY));
  response->print(F("<h1>WiFi Clock</h1>\n"
    "Uptime: "));
  response->print(millis() / 1000);
  response->print(F(" sec.</br>\n"
    "Free heap: "));
  response->print(ESP.getFreeHeap());
  response->print(F(" bytes</br>\n"));
#ifdef USE_SHT3X
  if (sht && (! isnan(temp)) && (! isnan(hum))) {
    response->printf_P(PSTR("SHT3x: %0.1f&deg; %0.1f%%</br>\n"), temp, hum);
  }
#endif
  response->print(F("<p>\n"
    "<a href='"));
  response->print(FPSTR(URL_WIFI));
  response->print(F("'>WiFi</a>\n"
    "<a href='"));
  response->print(FPSTR(URL_NTP));
  response->print(F("'>NTP</a>\n"
    "<a href='"));
  response->print(FPSTR(URL_LOG));
  response->print(F("'>Log</a>\n"));
/*
  response->print(F("<a href='"));
  response->print(FPSTR(URL_RESET));
  response->print('\'');
  if (WiFi.getMode() == WIFI_STA)
    response->print(F(" onclick='if(!confirm(\"Are you sure?\")) return false'"));
  response->print(F(">Reset!</a>\n"));
*/
  response->print(F("<a href='"));
  response->print(FPSTR(URL_RESTART));
  response->print('\'');
  if (WiFi.getMode() == WIFI_STA)
    response->print(F(" onclick='if(!confirm(\"Are you sure?\")) return false'"));
  response->print(F(">Restart!</a>\n"));
  response->print(FPSTR(HTML_PAGE_END));
  request->send(response);
}

static void webReset(AsyncWebServerRequest *request) {
  if (! webAuthorize(request))
    return;

  AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//  response->setCode(200);
  response->print(FPSTR(HTML_PAGE_START));
  response->print(F("Reset Config"));
  response->print(FPSTR(HTML_PAGE_CONT));
  response->print(F("<meta http-equiv=\"refresh\" content=\"3;URL=/\">\n"));
  response->print(FPSTR(HTML_STYLE_START));
  response->print(F("body{background-color:#eee}\n"));
  response->print(FPSTR(HTML_STYLE_END));
  response->print(FPSTR(HTML_BODY));
  response->print(F("Configuration cleared.</br>\n"
    "Don\'t forget to restart for the changes to take effect!\n"));
  response->print(FPSTR(HTML_PAGE_END));
  request->send(response);
  config.clear();
  logger.println(F("Configuration cleared"));
  if (! config) {
    if (! config.store()) {
      logger.println(F("Error storing configuration!"));
    }
  }
}

static void webRestart(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//  response->setCode(200);
  response->print(FPSTR(HTML_PAGE_START));
  response->print(F("Restart"));
  response->print(FPSTR(HTML_PAGE_CONT));
  response->print(F("<meta http-equiv=\"refresh\" content=\"5;URL=/\">\n"));
  response->print(FPSTR(HTML_STYLE_START));
  response->print(F("body{background-color:#eee}\n"));
  response->print(FPSTR(HTML_STYLE_END));
  response->print(FPSTR(HTML_BODY));
  response->print(F("Restarting...\n"));
  response->print(FPSTR(HTML_PAGE_END));
  request->send(response);
  restarting = true;
}

static void webOta(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (! index) {
    logger.printf_P(PSTR("OTA update start: %s\n"), filename.c_str());
    if (! Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
      Update.printError(logger);
    }
  }
  if (! Update.hasError()) {
    if (Update.write(data, len) != len) {
      Update.printError(logger);
    }
  }
  if (final) {
    if (Update.end(true)) {
      logger.printf_P(PSTR("Update success: %u B\n"), index + len);
    } else {
      Update.printError(logger);
    }
  }
}

static void webStoreConfig(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));
  bool success = true;

  if (! config) {
    success = config.store();
    if (! success) {
      logger.println(F("Error storing configuration!"));
    }
  }
  response->print(FPSTR(HTML_PAGE_START));
  response->print(F("Store configuration"));
  response->print(FPSTR(HTML_PAGE_CONT));
  response->print(F("<meta http-equiv=\"refresh\" content=\"3;URL=/\">\n"));
  response->print(FPSTR(HTML_STYLE_START));
  response->print(F("body{background-color:#eee"));
  if (! success) {
    response->setCode(400);
    response->print(F(";color:red"));
  }
  response->print(F("}\n"));
  response->print(FPSTR(HTML_STYLE_END));
  response->print(FPSTR(HTML_BODY));
  if (success) {
    response->print(F("Configuration stored.</br>\n"
      "Don\'t forget to restart for the changes to take effect!\n"));
  } else {
    response->print(F("Error storing configuration!\n"));
  }
  response->print(FPSTR(HTML_PAGE_END));
  request->send(response);
}

static void webWiFi(AsyncWebServerRequest *request) {
  if (! webAuthorize(request))
    return;

  if (request->method() == HTTP_GET) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//    response->setCode(200);
    response->print(FPSTR(HTML_PAGE_START));
    response->print(F("WiFi Setup"));
    response->print(FPSTR(HTML_PAGE_CONT));
    response->print(FPSTR(HTML_STYLE_START));
    response->print(F("body{background-color:#eee}\n"
      "td:first-child{text-align:right}\n"));
    response->print(FPSTR(HTML_STYLE_END));
    response->print(FPSTR(HTML_BODY));
    response->print(F("<h2>WiFi Setup</h2>\n"
      "<form method='post'>\n"
      "<table>\n"
      "<tr><td>WiFi SSID:</td><td><input type='text' name='"));
    response->print(FPSTR(PARAM_WIFI_SSID));
    response->print(F("' value='"));
    encodeString(response, config->wifi_ssid);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->wifi_ssid) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->wifi_ssid) - 1);
    response->print(F("></td></tr>\n"
      "<tr><td>WiFi password:</td><td><input type='password' name='"));
    response->print(FPSTR(PARAM_WIFI_PSWD));
    response->print(F("' value='"));
    encodeString(response, config->wifi_pswd);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->wifi_pswd) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->wifi_pswd) - 1);
    response->print(F("></td></tr>\n"
      "<tr><td colspan=2>&nbsp;</td></tr>\n"
      "<tr><td>Admin name:</td><td><input type='text' name='"));
    response->print(FPSTR(PARAM_ADM_NAME));
    response->print(F("' value='"));
    encodeString(response, config->adm_name);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->adm_name) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->adm_name) - 1);
    response->print(F("></td></tr>\n"
      "<tr><td>Admin password:</td><td><input type='password' name='"));
    response->print(FPSTR(PARAM_ADM_PSWD));
    response->print(F("' value='"));
    encodeString(response, config->adm_pswd);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->adm_pswd) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->adm_pswd) - 1);
    response->print(F("></td></tr>\n"
#ifdef USE_LLMNR
      "<tr><td colspan=2>&nbsp;</td></tr>\n"
      "<tr><td>LLMNR host name:</td><td><input type='text' name='"));
    response->print(FPSTR(PARAM_LLMNR_NAME));
    response->print(F("' value='"));
    encodeString(response, config->llmnr_name);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->llmnr_name) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->llmnr_name) - 1);
    response->print(F("></td></tr>\n"
#endif
      "</table>\n"
      "<input type='submit' value='Save'>\n"
      "<input type='button' value='Back' onclick='location.href=\"/\"'>\n"
      "</form>\n"));
    response->print(FPSTR(HTML_PAGE_END));
    request->send(response);
  } else if (request->method() == HTTP_POST) {
    AsyncWebParameter *param;

    if ((param = request->getParam(FPSTR(PARAM_WIFI_SSID), true)))
      strlcpy(config->wifi_ssid, param->value().c_str(), sizeof(config->wifi_ssid));
    if ((param = request->getParam(FPSTR(PARAM_WIFI_PSWD), true)))
      strlcpy(config->wifi_pswd, param->value().c_str(), sizeof(config->wifi_pswd));
    if ((param = request->getParam(FPSTR(PARAM_ADM_NAME), true)))
      strlcpy(config->adm_name, param->value().c_str(), sizeof(config->adm_name));
    if ((param = request->getParam(FPSTR(PARAM_ADM_PSWD), true)))
      strlcpy(config->adm_pswd, param->value().c_str(), sizeof(config->adm_pswd));
#ifdef USE_LLMNR
    if ((param = request->getParam(FPSTR(PARAM_LLMNR_NAME), true)))
      strlcpy(config->llmnr_name, param->value().c_str(), sizeof(config->llmnr_name));
#endif
    webStoreConfig(request);
  } else {
    request->send(405);
  }
}

static void webNtp(AsyncWebServerRequest *request) {
  if (! webAuthorize(request))
    return;

  if (request->method() == HTTP_GET) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//    response->setCode(200);
    response->print(FPSTR(HTML_PAGE_START));
    response->print(F("NTP Setup"));
    response->print(FPSTR(HTML_PAGE_CONT));
    response->print(FPSTR(HTML_STYLE_START));
    response->print(F("body{background-color:#eee}\n"
      "td:first-child{text-align:right}\n"));
    response->print(FPSTR(HTML_STYLE_END));
    response->print(FPSTR(HTML_BODY));
    response->print(F("<h2>NTP Setup</h2>\n"
      "<form method='post'>\n"
      "<table>\n"
      "<tr><td>NTP server:</td><td><input type='text' name='"));
    response->print(FPSTR(PARAM_NTP_SERVER));
    response->print(F("' value='"));
    encodeString(response, config->ntp_server);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->ntp_server) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->ntp_server) - 1);
    response->print(F("></td></tr>\n"
      "<tr><td>Time zone:</td><td><select name='"));
    response->print(FPSTR(PARAM_NTP_TZ));
    response->print(F("'>"));
    for (int8_t tz = -11; tz <= 13; ++tz) {
      response->print(F("<option value='"));
      response->print(tz);
      response->print('\'');
      if (config->ntp_tz == tz)
        response->print(F(" selected"));
      response->print(F(">GMT"));
      if (tz >= 0)
        response->print('+');
      response->print(tz);
      response->print(F("</option>"));
    }
    response->print(F("</select></td></tr>\n"
      "<tr><td>Update interval (sec.):</td><td><input type='number' name='"));
    response->print(FPSTR(PARAM_NTP_INTERVAL));
    response->print(F("' value='"));
    response->print(config->ntp_interval);
    response->print(F("' min=0 max=65535></td></tr>\n"
      "<tr><td colspan=2>&nbsp;</td></tr>\n"
      "<tr><td>Greetings:</td><td><input type='text' name='"));
    response->print(FPSTR(PARAM_GREETINGS));
    response->print(F("' value='"));
    encodeString(response, config->greetings);
    response->print(F("' size="));
    response->print(_min(TEXT_SIZE, sizeof(config->greetings) - 1));
    response->print(F(" maxlength="));
    response->print(sizeof(config->greetings) - 1);
    response->print(F("></td></tr>\n"
      "<tr><td>Morning hour:</td><td><input type='number' name='"));
    response->print(FPSTR(PARAM_MORNING_HOUR));
    response->print(F("' value='"));
    response->print(config->morning_hour);
    response->print(F("' min=0 max=23></td></tr>\n"
      "<tr><td>Morning brightness:</td><td><input type='number' name='"));
    response->print(FPSTR(PARAM_MORNING_BRIGHT));
    response->print(F("' value='"));
    response->print(config->morning_bright);
    response->print(F("' min=0 max=15></td></tr>\n"
      "<tr><td>Evening hour:</td><td><input type='number' name='"));
    response->print(FPSTR(PARAM_EVENING_HOUR));
    response->print(F("' value='"));
    response->print(config->evening_hour);
    response->print(F("' min=0 max=23></td></tr>\n"
      "<tr><td>Evening brightness:</td><td><input type='number' name='"));
    response->print(FPSTR(PARAM_EVENING_BRIGHT));
    response->print(F("' value='"));
    response->print(config->evening_bright);
    response->print(F("' min=0 max=15></td></tr>\n"
      "</table>\n"
      "<input type='submit' value='Save'>\n"
      "<input type='button' value='Back' onclick='location.href=\"/\"'>\n"
      "</form>\n"));
    response->print(FPSTR(HTML_PAGE_END));
    request->send(response);
  } else if (request->method() == HTTP_POST) {
    AsyncWebParameter *param;

    if ((param = request->getParam(FPSTR(PARAM_NTP_SERVER), true)))
      strlcpy(config->ntp_server, param->value().c_str(), sizeof(config->ntp_server));
    if ((param = request->getParam(FPSTR(PARAM_NTP_TZ), true)))
      config->ntp_tz = constrain(param->value().toInt(), -11, 13);
    if ((param = request->getParam(FPSTR(PARAM_NTP_INTERVAL), true)))
      config->ntp_interval = param->value().toInt();
    if ((param = request->getParam(FPSTR(PARAM_GREETINGS), true)))
      strlcpy(config->greetings, param->value().c_str(), sizeof(config->greetings));
    if ((param = request->getParam(FPSTR(PARAM_MORNING_HOUR), true)))
      config->morning_hour = constrain(param->value().toInt(), 0, 23);
    if ((param = request->getParam(FPSTR(PARAM_MORNING_BRIGHT), true)))
      config->morning_bright = constrain(param->value().toInt(), 0, 15);
    if ((param = request->getParam(FPSTR(PARAM_EVENING_HOUR), true)))
      config->evening_hour = constrain(param->value().toInt(), 0, 23);
    if ((param = request->getParam(FPSTR(PARAM_EVENING_BRIGHT), true)))
      config->evening_bright = constrain(param->value().toInt(), 0, 15);
    webStoreConfig(request);
    if (isEvening((ntpTime() / 3600) % 24))
      display.setBrightness(config->evening_bright);
    else
      display.setBrightness(config->morning_bright);
  } else {
    request->send(405);
  }
}

static void webLog(AsyncWebServerRequest *request) {
/*
  if (! webAuthorize(request))
    return;
*/

  if (request->method() == HTTP_GET) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//    response->setCode(200);
    response->print(FPSTR(HTML_PAGE_START));
    response->print(F("Log"));
    response->print(FPSTR(HTML_PAGE_CONT));
    response->print(FPSTR(HTML_STYLE_START));
    response->print(F("body{background-color:#eee}\n"
      "textarea{resize:none;overflow:auto;width:98%}\n"));
    response->print(FPSTR(HTML_STYLE_END));
    response->print(FPSTR(HTML_SCRIPT_START));
    response->print(F("function logScroll(){\n"
      "let l=document.getElementById('log');\n"
      "l.scrollTop=l.scrollHeight;\n"
      "}\n"));
    response->print(FPSTR(HTML_SCRIPT_END));
    response->print(FPSTR(HTML_BODY_START));
    response->print(F(" onload='logScroll()'>\n"
      "<h2>Log</h2>\n"
      "<textarea id='log' rows=25 readonly>\n"));
    encodeString(response, (const char*)logger);
    response->print(F("</textarea>\n"
      "<form method='post'>\n"
      "<input type='submit' value='Clear'>\n"
      "<input type='button' value='Back' onclick='location.href=\"/\"'>\n"
      "</form>\n"));
    response->print(FPSTR(HTML_PAGE_END));
    request->send(response);
  } else if (request->method() == HTTP_POST) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

//    response->setCode(200);
    response->print(FPSTR(HTML_PAGE_START));
    response->print(F("Clear Log"));
    response->print(FPSTR(HTML_PAGE_CONT));
    response->print(F("<meta http-equiv=\"refresh\" content=\"2;URL="));
    response->print(FPSTR(URL_LOG));
    response->print(F("\">\n"));
    response->print(FPSTR(HTML_STYLE_START));
    response->print(F("body{background-color:#eee}\n"));
    response->print(FPSTR(HTML_STYLE_END));
    response->print(FPSTR(HTML_BODY));
    response->print(F("Log cleared\n"));
    response->print(FPSTR(HTML_PAGE_END));
    request->send(response);
    logger.clear();
    logger.println(F("Log cleared"));
  } else {
    request->send(405);
  }
}

static bool captivePortal(uint32_t timeout = 0) {
  DNSServer dns;
  char ssid[sizeof(CP_SSID) + 6];
  char pswd[sizeof(CP_PSWD)];
  uint8_t mac[6];

  WiFi.macAddress(mac);
  sprintf_P(ssid, PSTR(CP_SSID"%02X%02X%02X"), mac[3], mac[4], mac[5]);
  strcpy_P(pswd, PSTR(CP_PSWD));
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(ssid, pswd, 13)) {
    logger.printf_P(PSTR("Please connect to CP \"%s\" with password \"%s\"\n"), ssid, pswd);
  } else {
    logger.println(F("CP init error!"));
    return false;
  }
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, F("*"), WiFi.softAPIP());
  http.begin();
#ifdef LED_PIN
  led.setMode(0, led.LED_CP0);
#endif
  {
    char str[64];

    sprintf_P(str, PSTR("Connect to \"%s\" with password \"%s\"..."), ssid, pswd);
    display.scroll(str, 50);
  }
  if (timeout) {
    uint32_t start = millis();

    while (millis() - start < timeout) {
      if (restarting) {
        delay(100);
        restart(F("Restarting"));
      }
      if (WiFi.softAPgetStationNum())
        start = millis();
      dns.processNextRequest();
      delay(1);
    }
  } else { // Infinite loop
    while (true) {
      if (restarting) {
        delay(100);
        restart(F("Restarting"));
      }
      dns.processNextRequest();
      delay(1);
    }
  }
  http.end();
  WiFi.softAPdisconnect();
#ifdef LED_PIN
  led.setMode(0, led.LED_OFF);
#endif
  display.noScroll();
  return true;
}

static uint32_t getRstCount() {
  uint32_t rst_count[2];

  ESP.rtcUserMemoryRead(0, rst_count, sizeof(rst_count));
  if (~rst_count[0] == rst_count[1])
    return rst_count[1];
  return 0;
}

static uint32_t addRstCount() {
  uint32_t rst_count[2];

  ESP.rtcUserMemoryRead(0, rst_count, sizeof(rst_count));
  if (~rst_count[0] == rst_count[1])
    ++rst_count[1];
  else
    rst_count[1] = 1;
  rst_count[0] = ~rst_count[1];
  ESP.rtcUserMemoryWrite(0, rst_count, sizeof(rst_count));
  return rst_count[1];
}

static void clearRstCount() {
  uint32_t rst_count[2];

  rst_count[1] = 0;
  rst_count[0] = ~rst_count[1];
  ESP.rtcUserMemoryWrite(0, rst_count, sizeof(rst_count));
}

void setup() {
  if (ESP.getResetInfoPtr()->reason == REASON_EXT_SYS_RST)
    addRstCount();

#ifdef USE_SERIAL
  Serial.begin(115200);
  Serial.println();
#endif

#ifdef LED_PIN
  led.add(LED_PIN, LED_LEVEL);
#endif

  if (! logger.begin())
    restart(F("Not enoung memory!"));

  if (! LittleFS.begin()) {
    if ((! LittleFS.format()) || (! LittleFS.begin()))
      restart(F("FS init fail!"));
  }

  config.onClear([&](config_t *cfg) {
#ifdef DEF_WIFI_SSID
    strlcpy_P(cfg->wifi_ssid, PSTR(DEF_WIFI_SSID), sizeof(config_t::wifi_ssid));
#endif
#ifdef DEF_WIFI_PSWD
    strlcpy_P(cfg->wifi_pswd, PSTR(DEF_WIFI_PSWD), sizeof(config_t::wifi_pswd));
#endif
#ifdef DEF_ADM_NAME
    strlcpy_P(cfg->adm_name, PSTR(DEF_ADM_NAME), sizeof(config_t::adm_name));
#endif
#ifdef DEF_ADM_PSWD
    strlcpy_P(cfg->adm_pswd, PSTR(DEF_ADM_PSWD), sizeof(config_t::adm_pswd));
#endif
#ifdef USE_LLMNR
#ifdef DEF_LLMNR_NAME
    strlcpy_P(cfg->llmnr_name, PSTR(DEF_LLMNR_NAME), sizeof(config_t::llmnr_name));
#endif
#endif
#ifdef DEF_NTP_SERVER
    strlcpy_P(cfg->ntp_server, PSTR(DEF_NTP_SERVER), sizeof(config_t::ntp_server));
#endif
#ifdef DEF_NTP_TZ
    cfg->ntp_tz = DEF_NTP_TZ;
#endif
#ifdef DEF_NTP_INTERVAL
    cfg->ntp_interval = DEF_NTP_INTERVAL;
#endif
    strlcpy_P(cfg->greetings, PSTR("\xC7\xE4\xF0\xE0\xE2\xF1\xF2\xE2\xF3\xE9\xF2\xE5!"), sizeof(config_t::greetings)); // "Здравствуйте!"
    cfg->morning_hour = 8;
    cfg->morning_bright = 4;
    cfg->evening_hour = 22;
//    cfg->evening_bright = 0;
  });
  config.begin();

#ifdef USE_SHT3X
  sht = new SHT3x<>();
  sht->init();
  if (! sht->begin()) {
    delete sht;
    sht = nullptr;
    logger.println(F("SHT3x not found!"));
  }
#endif

  display.init();
  display.begin(config->evening_bright);
  display.scroll(config->greetings, 50);
  delay(3000);
  display.noScroll();
  display.clear();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiOnEvent);

  http.onNotFound(webNotFound);
  http.on(URL_ROOT, HTTP_GET, webRoot);
  http.on(URL_RESET, HTTP_GET, webReset);
  http.on(URL_RESTART, HTTP_GET, webRestart);
  http.on(URL_OTA, HTTP_GET, [](AsyncWebServerRequest *request) {
    if (! webAuthorize(request))
      return;

    request->send_P(200, FPSTR(TEXT_HTML), PSTR("<form method='post' enctype='multipart/form-data'>\n"
      "<label>Firmware file:</label>\n"
      "<input type='file' name='update' accept='.bin'></br>\n"
      "<input type='submit' value='Update'>\n"
      "<input type='button' value='Back' onclick='history.back()'>\n"
      "</form>"));
  });
  http.on(URL_OTA, HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream(FPSTR(TEXT_HTML));

    response->print(FPSTR(HTML_PAGE_START));
    response->print(F("OTA"));
    response->print(FPSTR(HTML_PAGE_CONT));
    response->print(F("<meta http-equiv=\"refresh\" content=\"5;URL=/\">\n"));
    response->print(FPSTR(HTML_STYLE_START));
    response->print(F("body{background-color:#eee"));
    if (Update.hasError()) {
      response->setCode(400);
      response->print(F(";color:red"));
    }
    response->print(F("}\n"));
    response->print(FPSTR(HTML_STYLE_END));
    response->print(FPSTR(HTML_BODY));
    if (Update.hasError()) {
      response->print(F("OTA error!\n"));
    } else {
      response->print(F("OTA successful</br>\n"
        "Restarting...\n"));
    }
    response->print(FPSTR(HTML_PAGE_END));
    request->send(response);
    if (! Update.hasError()) {
      restarting = true;
    }
  }, webOta);
  http.on(URL_WIFI, HTTP_ANY, webWiFi);
  http.on(URL_NTP, HTTP_ANY, webNtp);
  http.on(URL_LOG, HTTP_ANY, webLog);
  http.serveStatic(URL_ROOT, LittleFS, URL_ROOT);

  if (getRstCount() >= RST_RESET) {
    config.clear();
    logger.println(F("Configuration cleared"));
    if (! config) {
      if (! config.store()) {
        logger.println(F("Error storing configuration!"));
      }
    }
    clearRstCount();
    if (! captivePortal())
      restart();
  } else if ((! *config->wifi_ssid) || (! *config->ntp_server) || (getRstCount() >= RST_CP)) {
    clearRstCount();
/*
    do {
      if (! captivePortal(60000)) // 60 sec.
        restart();
    } while (! *config->wifi_ssid);
*/
    if (! captivePortal())
      restart();
  }
  clearRstCount();

  if (*config->ntp_server)
    actions.add(ntpUpdating);
#ifdef USE_SHT3X
  if (sht)
    actions.add(shtUpdating);
#endif

#ifdef USE_LLMNR
  if (*config->llmnr_name) {
    if (! LLMNR.begin(config->llmnr_name))
      logger.println(F("LLMNR init fail!"));
  }
#endif

  wifiConnect();
}

void loop() {
  uint32_t t;

  if (restarting) {
    delay(100);
    restart(F("Restarting"));
  }
  if ((t = ntpTime())) {
    uint16_t y;
    uint8_t h, m, s, w, d, mo;

    parseEpoch(t, &h, &m, &s, &w, &d, &mo, &y);

    if (s < 50) {
      char str[12];

      if ((s <= 1) && (m == 0)) { // Beginning of hour
        if (h == config->morning_hour)
          display.setBrightness(config->morning_bright);
        else if (h == config->evening_hour)
          display.setBrightness(config->evening_bright);
      }
#ifdef USE_SHT3X
      if (sht && (! isnan(temp)) && (! isnan(hum)) && (((s >= 10) && (s < 20)) || ((s >= 30) && (s < 40)))) {
        sprintf_P(str, PSTR("%0.1f\xB0 %0.1f%%"), temp, hum);
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

      strcpy_P(str, WEEKDAYS[w]);
      sprintf_P(&str[strlen(str)], PSTR(" %02u.%02u.%u"), d, mo, y);
      display.scroll(str);
      delay((60 - t % 60) * 1000);
      display.noScroll();
    }
  }
  actions.loop();
}
