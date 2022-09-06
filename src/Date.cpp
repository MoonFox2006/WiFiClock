#include <pgmspace.h>
#include "Date.h"

static const uint8_t DAYS_IN_MONTH[] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char WEEKDAY_NAMES[][4] PROGMEM = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char MONTH_NAMES[][4] PROGMEM = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const uint16_t EPOCH_TIME_2000 = 10957; // Days from 01.01.1970 to 01.01.2000
const uint16_t EPOCH_TIME_2022 = 18993; // Days from 01.01.1970 to 01.01.2022

bool isLeapYear(uint16_t year) {
  return (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0));
}

uint8_t lastDayOfMonth(uint8_t month, uint16_t year) {
  uint8_t result = pgm_read_byte(DAYS_IN_MONTH + month - 1);

  if ((month == 2) && isLeapYear(year))
    ++result;
  return result;
}

void parseEpoch(uint32_t epoch, uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *weekday, uint8_t *day, uint8_t *month, uint16_t *year) {
  if (second)
    *second = epoch % 60;
  epoch /= 60;
  if (minute)
    *minute = epoch % 60;
  epoch /= 60;
  if (hour)
    *hour = epoch % 24;

  uint16_t days, y;
  bool leap;

  days = epoch / 24;
  if (weekday)
    *weekday = (days + 3) % 7; // 1 Jan 1970 is Thursday
  if (days >= EPOCH_TIME_2022) {
    y = 2022;
    days -= EPOCH_TIME_2022;
  } else if (days >= EPOCH_TIME_2000) {
    y = 2000;
    days -= EPOCH_TIME_2000;
  } else
    y = 1970;
  for (; ; ++y) {
    leap = isLeapYear(y);
    if (days < 365 + leap)
      break;
    days -= 365 + leap;
  }
  if (year)
    *year = y;
  for (y = 1; ; ++y) {
    uint8_t daysPerMonth = pgm_read_byte(DAYS_IN_MONTH + y - 1);

    if (leap && (y == 2))
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  if (month)
    *month = y;
  if (day)
    *day = days + 1;
}

uint32_t combineEpoch(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year) {
  uint16_t days = day - 1;
  uint16_t y;

  if (year >= 2022) {
    days += EPOCH_TIME_2022;
    y = 2022;
  } else if (year >= 2000) {
    days += EPOCH_TIME_2000;
    y = 2000;
  } else
    y = 1970;
  for (; y < year; ++y)
    days += 365 + isLeapYear(y);
  for (y = 1; y < month; ++y)
    days += pgm_read_byte(DAYS_IN_MONTH + y - 1);
  if ((month > 2) && isLeapYear(year))
    ++days;
  return (((uint32_t)days * 24 + hour) * 60 + minute) * 60 + second;
}
