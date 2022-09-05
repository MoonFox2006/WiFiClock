#pragma once

#include <functional>
#include <EEPROM.h>

template<typename T>
class Parameters {
public:
  typedef std::function<void(T*)> clearcb_t;

  Parameters() : _onclear(nullptr) {}

  void begin();
  bool check() const;
  operator bool() const {
    return check();
  }
  T* operator ->() {
    return ptr();
  }
  bool import(const uint8_t *data);
  bool store();
  void onClear(clearcb_t cb) {
    _onclear = cb;
  }
  void clear();

protected:
  static const uint16_t SIGN = 0xA3C5;

  T *ptr() {
    return (T*)&EEPROM.getDataPtr()[4];
  }

  clearcb_t _onclear;
};

static uint16_t crc16(uint8_t value, uint16_t crc = 0xFFFF) {
  crc ^= value;
  for (uint8_t i = 0; i < 8; ++i) {
    if (crc & 0x01)
      crc = (crc >> 1) ^ 0xA001;
    else
      crc >>= 1;
  }
  return crc;
}

static uint16_t crc16(const uint8_t *data, uint16_t size, uint16_t crc = 0xFFFF) {
  while (size--) {
    crc = crc16(*data++, crc);
  }
  return crc;
}

template<typename T>
void Parameters<T>::begin() {
  EEPROM.begin(sizeof(T) + 4);
  if (! check())
    clear();
}

template<typename T>
bool Parameters<T>::import(const uint8_t *data) {
  uint8_t *_data = EEPROM.getDataPtr();

  memcpy_P(&_data[4], data, sizeof(T));
  *(uint16_t*)&_data[0] = SIGN;
  *(uint16_t*)&_data[2] = crc16(&_data[4], sizeof(T));
  return EEPROM.commit();
}

template<typename T>
bool Parameters<T>::store() {
  uint8_t *data = EEPROM.getDataPtr();

  *(uint16_t*)&data[0] = SIGN;
  *(uint16_t*)&data[2] = crc16(&data[4], sizeof(T));
  return EEPROM.commit();
}

template<typename T>
void Parameters<T>::clear() {
  uint8_t *data = EEPROM.getDataPtr();

  memset(&data[4], 0, sizeof(T));
  if (_onclear)
    _onclear((T*)&data[4]);
}

template<typename T>
bool Parameters<T>::check() const {
  const uint8_t *data = EEPROM.getConstDataPtr();

  return (*(uint16_t*)&data[0] == SIGN) && (crc16(&data[4], sizeof(T)) == *(uint16_t*)&data[2]);
}
