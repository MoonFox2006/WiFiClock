#pragma once

#include <math.h>
#include <Wire.h>

template<TwoWire &WIRE = Wire>
class SHT3x {
public:
  static void init(int8_t sda, int8_t scl, bool fast = true);
  static void init(bool fast = true);

  bool begin(uint8_t addr = 0);
  bool reset();
  bool heater(bool on);
  bool measure(float *temp, float *hum);
  float getTemperature();
  float getHumidity();

protected:
  static uint8_t crc8(uint16_t data);

  uint8_t _addr;
};

template<TwoWire &WIRE>
void SHT3x<WIRE>::init(int8_t sda, int8_t scl, bool fast) {
  WIRE.begin(sda, scl);
  if (fast)
    WIRE.setClock(400000);
}

template<TwoWire &WIRE>
void SHT3x<WIRE>::init(bool fast) {
  WIRE.begin();
  if (fast)
    WIRE.setClock(400000);
}

template<TwoWire &WIRE>
bool SHT3x<WIRE>::begin(uint8_t addr) {
  if (! addr) { // Auto address (0x44 or 0x45)
    _addr = 0x44;
    if (reset())
      return true;
    _addr = 0x45;
    return reset();
  } else {
    _addr = addr;
    return reset();
  }
}

template<TwoWire &WIRE>
bool SHT3x<WIRE>::reset() {
  WIRE.beginTransmission(_addr);
  WIRE.write(0x30);
  WIRE.write(0xA2);
  return (WIRE.endTransmission() == 0);
}

template<TwoWire &WIRE>
bool SHT3x<WIRE>::heater(bool on) {
  WIRE.beginTransmission(_addr);
  WIRE.write(0x30);
  if (on)
    WIRE.write(0x6D);
  else
    WIRE.write(0x66);
  return (WIRE.endTransmission() == 0);
}

template<TwoWire &WIRE>
bool SHT3x<WIRE>::measure(float *temp, float *hum) {
  uint16_t data;
  bool error = false;

  WIRE.beginTransmission(_addr);
  WIRE.write(0x2C);
  WIRE.write(0x06);
  if (WIRE.endTransmission() != 0)
    return false;
  if (WIRE.requestFrom(_addr, (uint8_t)6) != 6)
    return false;
  data = (WIRE.read() << 8) | WIRE.read();
  if (crc8(data) != WIRE.read())
    error = true;
  if (temp) {
    if (error)
      *temp = NAN;
    else
      *temp = 175 * (data / 65535.0) - 45;
  }
  data = (WIRE.read() << 8) | WIRE.read();
  if (crc8(data) != WIRE.read())
    error = true;
  if (hum) {
    if (error)
      *hum = NAN;
    else
      *hum = 100 * (data / 65535.0);
  }
  return (! error);
}

template<TwoWire &WIRE>
float SHT3x<WIRE>::getTemperature() {
  float result;

  if (! measure(&result, nullptr))
    result = NAN;
  return result;
}

template<TwoWire &WIRE>
float SHT3x<WIRE>::getHumidity() {
  float result;

  if (! measure(nullptr, &result))
    result = NAN;
  return result;
}

template<TwoWire &WIRE>
uint8_t SHT3x<WIRE>::crc8(uint16_t data) {
  uint8_t result = 0xFF;

  for (int8_t i = 1; i >= 0; --i) {
    result ^= data >> (8 * i);
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (result & 0x80)
        result = (result << 1) ^ 0x31;
      else
        result <<= 1;
    }
  }
  return result;
}
