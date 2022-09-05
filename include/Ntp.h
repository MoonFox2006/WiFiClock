#pragma once

#include <IPAddress.h>

uint32_t ntpTime();
bool ntpUpdate(const IPAddress &ntp_server, int8_t tz, uint32_t timeout = 1000, uint8_t repeat = 1);
bool ntpUpdate(const char *ntp_server, int8_t tz, uint32_t timeout = 1000, uint8_t repeat = 1);
bool ntpUpdate_P(PGM_P ntp_server, int8_t tz, uint32_t timeout = 1000, uint8_t repeat = 1);
