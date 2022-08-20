#pragma once

#include <pgmspace.h>
#include <SPI.h>
#include <Ticker.h>

template<const int8_t CS_PIN, const uint8_t WIDTH = 1, SPIClass &_SPI = SPI>
class MAX7219 {
public:
  static const uint8_t FONT_HEIGHT = 8;
  static const uint8_t FONT_GAP = 1;

  MAX7219() : _ticker(Ticker()), _scrolling(nullptr) {}
  ~MAX7219() {
    end();
  }

  void init();
  void begin(uint8_t bright = 7);
  void end();
  void clear();
  uint8_t width() const {
    return WIDTH * 8;
  }
  uint8_t height() const {
    return 8;
  }
  uint8_t getBrightness() const {
    return _bright;
  }
  void setBrightness(uint8_t value);
  void beginUpdate();
  void endUpdate();
  void repaint();
  bool getPixel(uint8_t x, uint8_t y);
  void setPixel(uint8_t x, uint8_t y, bool color);
  void drawPattern(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pattern);
  void drawPattern(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *pattern);
  void printChar(uint8_t x, uint8_t y, char c);
  void printStr(uint8_t x, uint8_t y, const char *str);
  void scroll(const char *str, uint32_t tempo = 100);
  void noScroll();
  void animate(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t frames, const uint8_t *patterns, uint32_t tempo = 100);
  void noAnimate();
  uint8_t charWidth(char c);
  uint16_t strWidth(const char *str);

protected:
  static const uint8_t SCROLL_ANCHOR = 5;

  void beginTransaction();
  void endTransaction();
  void sendCommand(uint8_t cmd, uint8_t value, uint8_t target = 0xFF);
  char charNormalize(char c);
  const uint8_t *charPattern(char c);

  static void onTick(MAX7219 *_this);

  struct __attribute__((__packed__)) scrolling_t {
    uint16_t width;
    int16_t pos;
    uint8_t canvas[0];
  };
  struct __attribute__((__packed__)) animating_t {
    uint8_t x, y, w, h;
    uint8_t frames, frame;
    uint8_t patterns[0];
  };

  Ticker _ticker;
  uint8_t _bits[WIDTH * 8];
  union {
    scrolling_t *_scrolling;
    animating_t *_animating;
  };
  uint8_t _bright : 4;
  uint8_t _updating : 3;
  bool _anim : 1;

  static const uint8_t FONT[] PROGMEM;
  static const uint8_t CHAR_WIDTH[] PROGMEM;
};

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::init() {
  if (CS_PIN >= 0) {
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
  }
  _SPI.begin();
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::begin(uint8_t bright) {
  memset(_bits, 0, sizeof(_bits));
  _bright = bright & 0x0F;
  _updating = 0;
  for (uint8_t i = 1; i <= 8; ++i) {
    sendCommand(i, 0);
  }
  sendCommand(0x09, 0); // Decode Mode OFF
  sendCommand(0x0A, _bright); // Intensity
  sendCommand(0x0B, 7); // Scan Limit
  sendCommand(0x0F, 0); // Test OFF
  sendCommand(0x0C, 1); // Shutdown OFF
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::end() {
  noScroll();
  sendCommand(0x0C, 0); // Shutdown ON
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::clear() {
  memset(_bits, 0, sizeof(_bits));
  if (! _updating)
    repaint();
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::setBrightness(uint8_t value) {
  _bright = value & 0x0F;
  sendCommand(0x0A, _bright);
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::beginUpdate() {
  if (_updating < 7)
    ++_updating;
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::endUpdate() {
  if (_updating) {
    if (! --_updating)
      repaint();
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::repaint() {
  for (uint8_t i = 0; i < 8; ++i) {
    beginTransaction();
    for (int8_t j = WIDTH - 1; j >= 0; --j) {
      _SPI.transfer16(((8 - i) << 8) | _bits[j * 8 + i]);
    }
    endTransaction();
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
bool MAX7219<CS_PIN, WIDTH, _SPI>::getPixel(uint8_t x, uint8_t y) {
  return (_bits[(x / 8) * 8 + y] >> (x % 8)) & 0x01;
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::setPixel(uint8_t x, uint8_t y, bool color) {
  if ((x < width()) && (y < height())) {
    if (color)
      _bits[(x / 8) * 8 + y] |= (1 << (x % 8));
    else
      _bits[(x / 8) * 8 + y] &= ~(1 << (x % 8));
    if (! _updating) {
      sendCommand(8 - y, _bits[(x / 8) * 8 + y], x / 8);
    }
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::drawPattern(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pattern) {
  if ((x < width()) && (y < height())) {
    beginUpdate();
    if (x + w > width())
      w = width() - x;
    if (y + h > height())
      h = height() - y;
    for (uint8_t i = 0; i < w; ++i) {
      for (uint8_t j = 0; j < h; ++j) {
        setPixel(x + i, y + j, (pattern >> j) & 0x01);
      }
    }
    endUpdate();
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::drawPattern(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *pattern) {
  if ((x < width()) && (y < height())) {
    beginUpdate();
    if (x + w > width())
      w = width() - x;
    if (y + h > height())
      h = height() - y;
    for (uint8_t i = 0; i < w; ++i) {
      for (uint8_t j = 0; j < h; ++j) {
        setPixel(x + i, y + j, (pgm_read_byte(&pattern[i]) >> j) & 0x01);
      }
    }
    endUpdate();
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::printChar(uint8_t x, uint8_t y, char c) {
  drawPattern(x, y, charWidth(c), FONT_HEIGHT, charPattern(c));
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::printStr(uint8_t x, uint8_t y, const char *str) {
  beginUpdate();
  while (pgm_read_byte(str) && (x < width())) {
    printChar(x, y, pgm_read_byte(str));
    x += charWidth(pgm_read_byte(str++));
    if (x < width())
      drawPattern(x++, y, FONT_GAP, FONT_HEIGHT, (uint8_t)0);
  }
  endUpdate();
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::scroll(const char *str, uint32_t tempo) {
  uint16_t w;

  noScroll();
  w = strWidth(str);
  if (w <= width()) {
    printStr(0, 0, str);
  } else {
    _scrolling = (scrolling_t*)malloc(sizeof(scrolling_t) + w);
    if (_scrolling) {
      memset(_scrolling->canvas, 0, w);
      _scrolling->width = w;
      w = 0;
      while (pgm_read_byte(str)) {
        const uint8_t *pattern;
        char c;

        c = pgm_read_byte(str++);
        pattern = charPattern(c);
        for (uint8_t i = 0; i < charWidth(c); ++i) {
          _scrolling->canvas[w++] = pgm_read_byte(&pattern[i]);
        }
        w += FONT_GAP;
      }
      drawPattern(0, 0, width(), FONT_HEIGHT, _scrolling->canvas);
      _scrolling->pos = -SCROLL_ANCHOR + 1;
      _anim = false;
      _ticker.attach_ms(tempo, &MAX7219::onTick, this);
    }
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::noScroll() {
  _ticker.detach();
  if (_scrolling) {
    free(_scrolling);
    _scrolling = nullptr;
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::animate(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t frames, const uint8_t *patterns, uint32_t tempo) {
  noAnimate();
  _animating = (animating_t*)malloc(sizeof(animating_t) + frames * w);
  if (_animating) {
    _animating->x = x;
    _animating->y = y;
    _animating->w = w;
    _animating->h = h;
    _animating->frames = frames;
    memcpy_P(_animating->patterns, patterns, frames * w);
    drawPattern(x, y, w, h, patterns);
    _animating->frame = 1;
    _anim = true;
    _ticker.attach_ms(tempo, &MAX7219::onTick, this);
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
inline void MAX7219<CS_PIN, WIDTH, _SPI>::noAnimate() {
  noScroll();
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::beginTransaction() {
  if (CS_PIN >= 0)
    digitalWrite(CS_PIN, LOW);
  _SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::endTransaction() {
  _SPI.endTransaction();
  if (CS_PIN >= 0)
    digitalWrite(CS_PIN, HIGH);
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::sendCommand(uint8_t cmd, uint8_t value, uint8_t target) {
  beginTransaction();
  for (int8_t i = WIDTH - 1; i >= 0; --i) {
    if ((target == 0xFF) | (i == target)) {
      _SPI.transfer16(((cmd & 0x0F) << 8) | value);
    } else {
      _SPI.transfer16(0); // NOP
    }
  }
  endTransaction();
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
uint8_t MAX7219<CS_PIN, WIDTH, _SPI>::charWidth(char c) {
  return pgm_read_byte(&CHAR_WIDTH[charNormalize(c) - ' ']);
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
uint16_t MAX7219<CS_PIN, WIDTH, _SPI>::strWidth(const char *str) {
  uint16_t result = 0;

  while (pgm_read_byte(str)) {
    if (result)
      result += FONT_GAP;
    result += charWidth(pgm_read_byte(str++));
  }
  return result;
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
char MAX7219<CS_PIN, WIDTH, _SPI>::charNormalize(char c) {
  if (c < ' ')
    c = ' ';
  else if (c >= 127) {
    if (c == 168) // 'Ё'
      c = 127;
    else if (c == 184) // 'ё'
      c = 128;
    else if (c >= 192) // 'А'
      c -= 63;
    else
      c = ' ';
  }
  return c;
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
const uint8_t *MAX7219<CS_PIN, WIDTH, _SPI>::charPattern(char c) {
  const uint8_t *result = FONT;

  c = charNormalize(c);
  for (char _c = ' '; _c < c; ++_c) {
    result += pgm_read_byte(&CHAR_WIDTH[_c - ' ']);
  }
  return result;
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
void MAX7219<CS_PIN, WIDTH, _SPI>::onTick(MAX7219 *_this) {
  if (_this->_anim) {
    _this->drawPattern(_this->_animating->x, _this->_animating->y, _this->_animating->w, _this->_animating->h, &_this->_animating->patterns[_this->_animating->frame * _this->_animating->w]);
    if (++_this->_animating->frame >= _this->_animating->frames)
      _this->_animating->frame = 0;
  } else {
    _this->drawPattern(0, 0, WIDTH * 8, _this->FONT_HEIGHT, &_this->_scrolling->canvas[constrain(_this->_scrolling->pos, 0, _this->_scrolling->width - WIDTH * 8)]);
    if (++_this->_scrolling->pos >= _this->_scrolling->width - WIDTH * 8 + SCROLL_ANCHOR)
      _this->_scrolling->pos = -SCROLL_ANCHOR;
  }
}

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
const uint8_t MAX7219<CS_PIN, WIDTH, _SPI>::FONT[] PROGMEM = {
  0x00, 0x00, 0x00, // ' '
  0x5F, // '!'
  0x07, 0x00, 0x07, // '"'
  0x14, 0x3E, 0x14, 0x3E, 0x14, // '#'
  0x24, 0x2A, 0x7F, 0x2A, 0x12, // '$'
  0x23, 0x13, 0x08, 0x64, 0x62, // '%'
  0x36, 0x49, 0x55, 0x22, 0x50, // '&'
  0x04, 0x02, 0x01, // '\''
  0x1C, 0x22, 0x41, // '('
  0x41, 0x22, 0x1C, // ')'
  0x14, 0x08, 0x3E, 0x08, 0x14, // '*'
  0x08, 0x08, 0x3E, 0x08, 0x08, // '+'
  0xA0, 0x60, // ','
  0x08, 0x08, 0x08, 0x08, 0x08, // '-'
  0x60, 0x60, // '.'
  0x20, 0x10, 0x08, 0x04, 0x02, // '/'
  0x3E, 0x51, 0x49, 0x45, 0x3E, // '0'
  0x04, 0x42, 0x7F, 0x40, // '1'
  0x62, 0x51, 0x49, 0x49, 0x46, // '2'
  0x22, 0x41, 0x49, 0x49, 0x36, // '3'
  0x18, 0x14, 0x12, 0x7F, 0x10, // '4'
  0x27, 0x45, 0x45, 0x45, 0x39, // '5'
  0x3C, 0x4A, 0x49, 0x49, 0x30, // '6'
  0x01, 0x71, 0x09, 0x05, 0x03, // '7'
  0x36, 0x49, 0x49, 0x49, 0x36, // '8'
  0x06, 0x49, 0x49, 0x29, 0x1E, // '9'
  0x36, 0x36, // ':'
  0x56, 0x36, // ';'
  0x08, 0x14, 0x22, 0x41, // '<'
  0x14, 0x14, 0x14, 0x14, 0x14, // '='
  0x41, 0x22, 0x14, 0x08, // '>'
  0x02, 0x01, 0x51, 0x09, 0x06, // '?'
  0x32, 0x49, 0x79, 0x41, 0x3E, // '@'
  0x7C, 0x12, 0x11, 0x12, 0x7C, // 'A'
  0x41, 0x7F, 0x49, 0x49, 0x36, // 'B'
  0x3E, 0x41, 0x41, 0x41, 0x22, // 'C'
  0x41, 0x7F, 0x41, 0x41, 0x3E, // 'D'
  0x7F, 0x49, 0x49, 0x49, 0x41, // 'E'
  0x7F, 0x09, 0x09, 0x09, 0x01, // 'F'
  0x3E, 0x41, 0x41, 0x51, 0x72, // 'G'
  0x7F, 0x08, 0x08, 0x08, 0x7F, // 'H'
  0x41, 0x7F, 0x41, // 'I'
  0x20, 0x40, 0x41, 0x3F, 0x01, // 'J'
  0x7F, 0x08, 0x14, 0x22, 0x41, // 'K'
  0x7F, 0x40, 0x40, 0x40, 0x40, // 'L'
  0x7F, 0x02, 0x0C, 0x02, 0x7F, // 'M'
  0x7F, 0x04, 0x08, 0x10, 0x7F, // 'N'
  0x3E, 0x41, 0x41, 0x41, 0x3E, // 'O'
  0x7F, 0x09, 0x09, 0x09, 0x06, // 'P'
  0x3E, 0x41, 0x51, 0x21, 0x5E, // 'Q'
  0x7F, 0x09, 0x19, 0x29, 0x46, // 'R'
  0x26, 0x49, 0x49, 0x49, 0x32, // 'S'
  0x01, 0x01, 0x7F, 0x01, 0x01, // 'T'
  0x3F, 0x40, 0x40, 0x40, 0x3F, // 'U'
  0x1F, 0x20, 0x40, 0x20, 0x1F, // 'V'
  0x3F, 0x40, 0x38, 0x40, 0x3F, // 'W'
  0x63, 0x14, 0x08, 0x14, 0x63, // 'X'
  0x07, 0x08, 0x70, 0x08, 0x07, // 'Y'
  0x61, 0x51, 0x49, 0x45, 0x43, // 'Z'
  0x7F, 0x41, 0x41, // '['
  0x02, 0x04, 0x08, 0x10, 0x20, // '\\'
  0x41, 0x41, 0x7F, // ']'
  0x04, 0x02, 0x01, 0x02, 0x04, // '^'
  0x80, 0x80, 0x80, 0x80, 0x80, // '_'
  0x01, 0x02, 0x04, // '`'
  0x20, 0x54, 0x54, 0x54, 0x78, // 'a'
  0x7F, 0x48, 0x44, 0x44, 0x38, // 'b'
  0x38, 0x44, 0x44, 0x44, 0x20, // 'c'
  0x38, 0x44, 0x44, 0x48, 0x7F, // 'd'
  0x38, 0x54, 0x54, 0x54, 0x18, // 'e'
  0x08, 0x7E, 0x09, 0x01, 0x02, // 'f'
  0x18, 0xA4, 0xA4, 0xA4, 0x7C, // 'g'
  0x7F, 0x08, 0x04, 0x04, 0x78, // 'h'
  0x48, 0x7D, 0x40, // 'i'
  0x20, 0x40, 0x44, 0x3D, // 'j'
  0x7F, 0x10, 0x28, 0x44, // 'k'
  0x41, 0x7F, 0x40, // 'l'
  0x7C, 0x04, 0x78, 0x04, 0x78, // 'm'
  0x7C, 0x08, 0x04, 0x04, 0x78, // 'n'
  0x38, 0x44, 0x44, 0x44, 0x38, // 'o'
  0xFC, 0x24, 0x24, 0x24, 0x18, // 'p'
  0x18, 0x24, 0x24, 0x28, 0xFC, // 'q'
  0x7C, 0x08, 0x04, 0x04, 0x08, // 'r'
  0x48, 0x54, 0x54, 0x54, 0x20, // 's'
  0x04, 0x3F, 0x44, 0x40, 0x20, // 't'
  0x3C, 0x40, 0x40, 0x20, 0x7C, // 'u'
  0x1C, 0x20, 0x40, 0x20, 0x1C, // 'v'
  0x3C, 0x40, 0x30, 0x40, 0x3C, // 'w'
  0x44, 0x28, 0x10, 0x28, 0x44, // 'x'
  0x1C, 0xA0, 0xA0, 0x90, 0x7C, // 'y'
  0x44, 0x64, 0x54, 0x4C, 0x44, // 'z'
  0x08, 0x36, 0x41, // '{'
  0x7F, // '|'
  0x41, 0x36, 0x08, // '}'
  0x08, 0x04, 0x04, 0x08, 0x04, // '~'

  0x7C, 0x55, 0x54, 0x55, 0x44, // 'Ё'
  0x38, 0x55, 0x54, 0x55, 0x18, // 'ё'
  0x7E, 0x11, 0x11, 0x11, 0x7E, // 'А'
  0x7F, 0x49, 0x49, 0x49, 0x33, // 'Б'
  0x7F, 0x49, 0x49, 0x49, 0x36, // 'В'
  0x7F, 0x01, 0x01, 0x01, 0x03, // 'Г'
  0x70, 0x29, 0x27, 0x21, 0x7F, // 'Д'
  0x7F, 0x49, 0x49, 0x49, 0x41, // 'Е'
  0x77, 0x08, 0x7F, 0x08, 0x77, // 'Ж'
  0x41, 0x41, 0x49, 0x49, 0x36, // 'З'
  0x7F, 0x10, 0x08, 0x04, 0x7F, // 'И'
  0x7C, 0x21, 0x12, 0x09, 0x7C, // 'Й'
  0x7F, 0x08, 0x14, 0x22, 0x41, // 'К'
  0x20, 0x41, 0x3F, 0x01, 0x7F, // 'Л'
  0x7F, 0x02, 0x0C, 0x02, 0x7F, // 'М'
  0x7F, 0x08, 0x08, 0x08, 0x7F, // 'Н'
  0x3E, 0x41, 0x41, 0x41, 0x3E, // 'О'
  0x7F, 0x01, 0x01, 0x01, 0x7F, // 'П'
  0x7F, 0x09, 0x09, 0x09, 0x06, // 'Р'
  0x3E, 0x41, 0x41, 0x41, 0x22, // 'С'
  0x01, 0x01, 0x7F, 0x01, 0x01, // 'Т'
  0x47, 0x28, 0x10, 0x08, 0x07, // 'У'
  0x1C, 0x22, 0x7F, 0x22, 0x1C, // 'Ф'
  0x63, 0x14, 0x08, 0x14, 0x63, // 'Х'
  0x7F, 0x40, 0x40, 0x7F, 0xC0, // 'Ц'
  0x07, 0x08, 0x08, 0x08, 0x7F, // 'Ч'
  0x7F, 0x40, 0x7F, 0x40, 0x7F, // 'Ш'
  0x7F, 0x40, 0x7F, 0x40, 0xFF, // 'Щ'
  0x01, 0x7F, 0x48, 0x48, 0x30, // 'Ъ'
  0x7F, 0x48, 0x30, 0x00, 0x7F, // 'Ы'
  0x7F, 0x48, 0x48, 0x48, 0x30, // 'Ь'
  0x22, 0x49, 0x45, 0x49, 0x3E, // 'Э'
  0x7F, 0x08, 0x3E, 0x41, 0x3E, // 'Ю'
  0x46, 0x29, 0x19, 0x09, 0x7F, // 'Я'
  0x20, 0x54, 0x54, 0x54, 0x78, // 'а'
  0x3C, 0x4A, 0x49, 0x49, 0x31, // 'б'
  0x7C, 0x54, 0x54, 0x54, 0x28, // 'в'
  0x7C, 0x04, 0x04, 0x04, 0x04, // 'г'
  0x60, 0x38, 0x24, 0x24, 0x7C, // 'д'
  0x38, 0x54, 0x54, 0x54, 0x18, // 'е'
  0x6C, 0x10, 0x7C, 0x10, 0x6C, // 'ж'
  0x44, 0x54, 0x54, 0x54, 0x28, // 'з'
  0x7C, 0x20, 0x10, 0x08, 0x7C, // 'и'
  0x78, 0x42, 0x24, 0x12, 0x78, // 'й'
  0x7C, 0x10, 0x28, 0x44, // 'к'
  0x40, 0x3C, 0x04, 0x04, 0x7C, // 'л'
  0x7C, 0x08, 0x10, 0x08, 0x7C, // 'м'
  0x7C, 0x10, 0x10, 0x10, 0x7C, // 'н'
  0x38, 0x44, 0x44, 0x44, 0x38, // 'о'
  0x7C, 0x04, 0x04, 0x04, 0x7C, // 'п'
  0xFC, 0x24, 0x24, 0x24, 0x18, // 'р'
  0x38, 0x44, 0x44, 0x44, 0x20, // 'с'
  0x04, 0x04, 0x7C, 0x04, 0x04, // 'т'
  0x44, 0x28, 0x10, 0x08, 0x04, // 'у'
  0x18, 0x24, 0xFE, 0x24, 0x18, // 'ф'
  0x44, 0x28, 0x10, 0x28, 0x44, // 'х'
  0x7C, 0x40, 0x40, 0x7C, 0xC0, // 'ц'
  0x0C, 0x10, 0x10, 0x10, 0x7C, // 'ч'
  0x7C, 0x40, 0x7C, 0x40, 0x7C, // 'ш'
  0x7C, 0x40, 0x7C, 0x40, 0xFC, // 'щ'
  0x04, 0x7C, 0x50, 0x50, 0x20, // 'ъ'
  0x7C, 0x50, 0x20, 0x00, 0x7C, // 'ы'
  0x7C, 0x50, 0x50, 0x50, 0x20, // 'ь'
  0x44, 0x54, 0x54, 0x54, 0x38, // 'э'
  0x7C, 0x10, 0x38, 0x44, 0x38, // 'ю'
  0x48, 0x54, 0x34, 0x14, 0x7C, // 'я'
};

template<const int8_t CS_PIN, const uint8_t WIDTH, SPIClass &_SPI>
const uint8_t MAX7219<CS_PIN, WIDTH, _SPI>::CHAR_WIDTH[] PROGMEM = {
  3, // ' '
  1, // '!'
  3, // '"'
  5, // '#'
  5, // '$'
  5, // '%'
  5, // '&'
  3, // '\''
  3, // '('
  3, // ')'
  5, // '*'
  5, // '+'
  2, // ','
  5, // '-'
  2, // '.'
  5, // '/'
  5, // '0'
  4, // '1'
  5, // '2'
  5, // '3'
  5, // '4'
  5, // '5'
  5, // '6'
  5, // '7'
  5, // '8'
  5, // '9'
  2, // ':'
  2, // ';'
  4, // '<'
  5, // '='
  4, // '>'
  5, // '?'
  5, // '@'
  5, // 'A'
  5, // 'B'
  5, // 'C'
  5, // 'D'
  5, // 'E'
  5, // 'F'
  5, // 'G'
  5, // 'H'
  3, // 'I'
  5, // 'J'
  5, // 'K'
  5, // 'L'
  5, // 'M'
  5, // 'N'
  5, // 'O'
  5, // 'P'
  5, // 'Q'
  5, // 'R'
  5, // 'S'
  5, // 'T'
  5, // 'U'
  5, // 'V'
  5, // 'W'
  5, // 'X'
  5, // 'Y'
  5, // 'Z'
  3, // '['
  5, // '\\'
  3, // ']'
  5, // '^'
  5, // '_'
  3, // '`'
  5, // 'a'
  5, // 'b'
  5, // 'c'
  5, // 'd'
  5, // 'e'
  5, // 'f'
  5, // 'g'
  5, // 'h'
  3, // 'i'
  4, // 'j'
  4, // 'k'
  3, // 'l'
  5, // 'm'
  5, // 'n'
  5, // 'o'
  5, // 'p'
  5, // 'q'
  5, // 'r'
  5, // 's'
  5, // 't'
  5, // 'u'
  5, // 'v'
  5, // 'w'
  5, // 'x'
  5, // 'y'
  5, // 'z'
  3, // '{'
  1, // '|'
  3, // '}'
  5, // '~'

  5, // 'Ё'
  5, // 'ё'
  5, // 'А'
  5, // 'Б'
  5, // 'В'
  5, // 'Г'
  5, // 'Д'
  5, // 'Е'
  5, // 'Ж'
  5, // 'З'
  5, // 'И'
  5, // 'Й'
  5, // 'К'
  5, // 'Л'
  5, // 'М'
  5, // 'Н'
  5, // 'О'
  5, // 'П'
  5, // 'Р'
  5, // 'С'
  5, // 'Т'
  5, // 'У'
  5, // 'Ф'
  5, // 'Х'
  5, // 'Ц'
  5, // 'Ч'
  5, // 'Ш'
  5, // 'Щ'
  5, // 'Ъ'
  5, // 'Ы'
  5, // 'Ь'
  5, // 'Э'
  5, // 'Ю'
  5, // 'Я'
  5, // 'а'
  5, // 'б'
  5, // 'в'
  5, // 'г'
  5, // 'д'
  5, // 'е'
  5, // 'ж'
  5, // 'з'
  5, // 'и'
  5, // 'й'
  4, // 'к'
  5, // 'л'
  5, // 'м'
  5, // 'н'
  5, // 'о'
  5, // 'п'
  5, // 'р'
  5, // 'с'
  5, // 'т'
  5, // 'у'
  5, // 'ф'
  5, // 'х'
  5, // 'ц'
  5, // 'ч'
  5, // 'ш'
  5, // 'щ'
  5, // 'ъ'
  5, // 'ы'
  5, // 'ь'
  5, // 'э'
  5, // 'ю'
  5, // 'я'
};
