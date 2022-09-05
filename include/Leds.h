#pragma once

#include <Arduino.h>
#include <Ticker.h>

template<const uint8_t CAPACITY>
class Leds {
public:
  enum ledmode_t { LED_OFF, LED_ON, LED_TOGGLE, LED_PWM, LED_05HZ, LED_1HZ, LED_2HZ, LED_4HZ, LED_FADEIN, LED_FADEOUT, LED_BREATH };

  Leds() : _ticker(Ticker()), _count(0), _pulses(0) {
    analogWriteRange(255);
  }
  ~Leds() {
    clear();
  }

  void clear();
  uint8_t count() const {
    return _count;
  }
  int8_t add(uint8_t pin, bool level, ledmode_t mode = LED_OFF);
  void remove(uint8_t index);
  int8_t find(uint8_t pin) const;
  ledmode_t getMode(uint8_t index) const;
  void setMode(uint8_t index, ledmode_t mode, bool force = false);
  uint8_t getDuty(uint8_t index) const;
  void setDuty(uint8_t index, uint8_t duty);

protected:
  struct __attribute__((__packed__)) led_t {
    uint8_t pin : 7;
    bool level : 1;
    ledmode_t mode : 4;
    uint8_t duty;
  };

  bool timerNeeded();

  static void tickerCallback(Leds *_this);

  led_t _items[CAPACITY];
  Ticker _ticker;
  uint8_t _count;
  volatile uint8_t _pulses;
};

template<const uint8_t CAPACITY>
void Leds<CAPACITY>::clear() {
  _ticker.detach();
  for (uint8_t i = 0; i < _count; ++i) {
    if ((_items[i].mode == LED_PWM) || (_items[i].mode >= LED_FADEIN)) {
      analogWrite(_items[i].pin, 0);
    }
    pinMode(_items[i].pin, INPUT);
  }
  _count = 0;
}

template<const uint8_t CAPACITY>
int8_t Leds<CAPACITY>::add(uint8_t pin, bool level, ledmode_t mode) {
  if (_count < CAPACITY) {
    _items[_count].pin = pin;
    _items[_count].level = level;
    _items[_count].mode = mode;
    _items[_count].duty = 0;
    pinMode(pin, OUTPUT);
    setMode(_count++, mode, true);
    return _count - 1;
  }
  return -1;
}

template<const uint8_t CAPACITY>
void Leds<CAPACITY>::remove(uint8_t index) {
  if (index < _count) {
    if ((_items[index].mode == LED_PWM) || (_items[index].mode >= LED_FADEIN)) {
      analogWrite(_items[index].pin, 0);
    }
    pinMode(_items[index].pin, INPUT);
    if (index < _count - 1)
      memmove(&_items[index], &_items[index + 1], sizeof(led_t) * (_count - index - 1));
    --_count;
    if (! timerNeeded())
      _ticker.detach();
  }
}

template<const uint8_t CAPACITY>
int8_t Leds<CAPACITY>::find(uint8_t pin) const {
  for (uint8_t i = 0; i < _count; ++i) {
    if (_items[i].pin == pin)
      return i;
  }
  return -1;
}

template<const uint8_t CAPACITY>
inline typename Leds<CAPACITY>::ledmode_t Leds<CAPACITY>::getMode(uint8_t index) const {
  return _items[index].mode;
}

template<const uint8_t CAPACITY>
void Leds<CAPACITY>::setMode(uint8_t index, ledmode_t mode, bool force) {
  if (index < _count) {
    if (force || (_items[index].mode != mode)) {
      if (mode == LED_OFF)
        digitalWrite(_items[index].pin, ! _items[index].level);
      else if (mode == LED_ON)
        digitalWrite(_items[index].pin, _items[index].level);
      else if (mode == LED_TOGGLE)
        digitalWrite(_items[index].pin, ! digitalRead(_items[index].pin));
      else if (mode == LED_PWM)
        analogWrite(_items[index].pin, _items[index].duty);
      _items[index].mode = mode;
      _ticker.detach();
      if (timerNeeded())
        _ticker.attach_ms(50, &Leds::tickerCallback, this); // 50 ms.
    } else if (mode == LED_TOGGLE) {
      digitalWrite(_items[index].pin, ! digitalRead(_items[index].pin));
    }
  }
}

template<const uint8_t CAPACITY>
uint8_t Leds<CAPACITY>::getDuty(uint8_t index) const {
  if ((index < _count) && (_items[index].mode == LED_PWM)) {
    return _items[index].duty;
  }
  return 0;
}

template<const uint8_t CAPACITY>
void Leds<CAPACITY>::setDuty(uint8_t index, uint8_t duty) {
  if ((index < _count) && (_items[index].mode == LED_PWM)) {
    analogWrite(_items[index].pin, duty);
    _items[index].duty = duty;
  }
}

template<const uint8_t CAPACITY>
bool Leds<CAPACITY>::timerNeeded() {
  for (uint8_t i = 0; i < _count; ++i) {
    if (_items[i].mode >= LED_05HZ)
      return true;
  }
  return false;
}

template<const uint8_t CAPACITY>
void Leds<CAPACITY>::tickerCallback(Leds *_this) {
  static const uint8_t SINUS[] PROGMEM = { 0, 1, 2, 8, 17, 30, 46, 64, 84, 105, 128, 150, 171, 191, 209, 225, 238, 247, 253, 255 };

  for (uint8_t i = 0; i < _this->_count; ++i) {
    if (_this->_items[i].mode >= LED_05HZ) {
      if (_this->_items[i].mode == LED_05HZ) {
        digitalWrite(_this->_items[i].pin, (_this->_pulses == 0) == _this->_items[i].level);
      } else if (_this->_items[i].mode == LED_1HZ) {
        digitalWrite(_this->_items[i].pin, ((_this->_pulses % 20) == 0) == _this->_items[i].level);
      } else if (_this->_items[i].mode == LED_2HZ) {
        digitalWrite(_this->_items[i].pin, ((_this->_pulses % 10) == 0) == _this->_items[i].level);
      } else if (_this->_items[i].mode == LED_4HZ) {
        digitalWrite(_this->_items[i].pin, ((_this->_pulses % 5) == 0) == _this->_items[i].level);
      } else {
        if (_this->_items[i].mode == LED_FADEIN)
          _this->_items[i].duty = pgm_read_byte(&SINUS[_this->_pulses % 20]);
        else if (_this->_items[i].mode == LED_FADEOUT)
          _this->_items[i].duty = pgm_read_byte(&SINUS[19 - _this->_pulses % 20]);
        else if (_this->_items[i].mode == LED_BREATH)
          _this->_items[i].duty = _this->_pulses >= 20 ? pgm_read_byte(&SINUS[19 - _this->_pulses % 20]) : pgm_read_byte(&SINUS[_this->_pulses % 20]);
        analogWrite(_this->_items[i].pin, _this->_items[i].duty);
      }
    }
  }
  if (++_this->_pulses >= 40)
    _this->_pulses = 0;
}
