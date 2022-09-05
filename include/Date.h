#pragma once

#include <inttypes.h>

bool isLeapYear(uint16_t year);
uint8_t lastDayOfMonth(uint8_t month, uint16_t year);

void parseEpoch(uint32_t epoch, uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *weekday, uint8_t *day, uint8_t *month, uint16_t *year);
uint32_t combineEpoch(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year);
