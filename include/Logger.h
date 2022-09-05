#pragma once

#include <Print.h>

template<const uint16_t MAX_SIZE = 4096>
class Logger : public Print {
public:
  Logger(Print *dup = nullptr) : Print(), _dup(dup), _buffer(nullptr) {}
  ~Logger() {
    if (_buffer)
      delete _buffer;
  }

  bool begin();
  void clear();
  uint16_t length() const;
  operator const char*();
  size_t write(uint8_t val);

protected:
  Print *_dup;
  char *_buffer;
  uint16_t _length;
};

template<const uint16_t MAX_SIZE>
bool Logger<MAX_SIZE>::begin() {
  if (! _buffer) {
    _buffer = new char[MAX_SIZE];
    if (! _buffer)
      return false;
  }
  clear();
  return true;
}

template<const uint16_t MAX_SIZE>
void Logger<MAX_SIZE>::clear() {
  *_buffer = '\0';
  _length = 0;
}

template<const uint16_t MAX_SIZE>
inline uint16_t Logger<MAX_SIZE>::length() const {
  return _length;
}

template<const uint16_t MAX_SIZE>
inline Logger<MAX_SIZE>::operator const char* () {
  return _buffer;
}

template<const uint16_t MAX_SIZE>
size_t Logger<MAX_SIZE>::write(uint8_t val) {
  if (val != '\r') {
    if (_length >= MAX_SIZE - 1) {
      uint16_t start = 0;

      while ((start < MAX_SIZE / 2) && (_buffer[start] != '\n')) // Find EOL
        ++start;
      ++start; // Skip '\n'
      _length -= start;
      memmove(_buffer, &_buffer[start], _length);
    }
    _buffer[_length++] = val;
    _buffer[_length] = '\0';
  }
  if (_dup)
    _dup->write(val);
  return sizeof(val);
}
