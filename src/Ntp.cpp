#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "Ntp.h"

static uint32_t _ntp_time = 0;
static uint32_t _ntp_updated = 0;

uint32_t ntpTime() {
  if (_ntp_time) {
    return _ntp_time + (millis() - _ntp_updated) / 1000;
  }
  return 0;
}

bool ntpUpdate(const IPAddress &ntp_server, int8_t tz, uint32_t timeout, uint8_t repeat) {
  const uint16_t LOCAL_PORT = 55123;

  if (WiFi.isConnected()) {
    WiFiUDP udp;

    if (udp.begin(LOCAL_PORT)) {
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
        if (udp.beginPacket(ntp_server, 123) && (udp.write(buffer, sizeof(buffer)) == sizeof(buffer)) && udp.endPacket()) {
          uint32_t time = millis();
          int cb;

          while ((! (cb = udp.parsePacket())) && (millis() - time < timeout)) {
            delay(1);
          }
          if (cb) {
            // We've received a packet, read the data from it
            if (udp.read(buffer, sizeof(buffer)) == sizeof(buffer)) { // read the packet into the buffer
              // the timestamp starts at byte 40 of the received packet and is four bytes,
              // or two words, long. First, esxtract the two words:
              _ntp_updated = millis();
              _ntp_time = (((uint32_t)buffer[40] << 24) | ((uint32_t)buffer[41] << 16) | ((uint32_t)buffer[42] << 8) | buffer[43]) - 2208988800UL;
              _ntp_time += tz * 3600;
              return true;
            }
          }
        }
      } while (repeat--);
    }
  }
  return false;
}

bool ntpUpdate(const char *ntp_server, int8_t tz, uint32_t timeout, uint8_t repeat) {
  IPAddress ip;

  if (WiFi.hostByName(ntp_server, ip)) {
    return ntpUpdate(ip, tz, timeout, repeat);
  }
  return 0;
}

bool ntpUpdate_P(PGM_P ntp_server, int8_t tz, uint32_t timeout, uint8_t repeat) {
  char _ntp_server[strlen_P(ntp_server) + 1];

  strcpy_P(_ntp_server, ntp_server);
  return ntpUpdate(_ntp_server, tz, timeout, repeat);
}
