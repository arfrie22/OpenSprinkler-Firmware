#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#if defined(ESP8266)

#include <SSD1306.h>

#include "font.h"
#include "images.h"

#define LCD_STD 0  // Standard LCD
#define LCD_I2C 1

class SSD1306Display : public SSD1306 {
 public:
  SSD1306Display(uint8_t _addr, uint8_t _sda, uint8_t _scl)
      : SSD1306(_addr, _sda, _scl) {
    cx = 0;
    cy = 0;
    for (byte i = 0; i < NUM_CUSTOM_ICONS; i++) custom_chars[i] = NULL;
  }
  void begin() {
    Wire.setClock(400000L);  // lower clock to 400kHz
    flipScreenVertically();
    setFont(Monospaced_plain_13);
    fontWidth = 8;
    fontHeight = 16;
  }
  void clear() { SSD1306::clear(); }
  void clear(int start, int end) {
    setColor(BLACK);
    fillRect(0, (start + 1) * fontHeight, 128, (end - start + 1) * fontHeight);
    setColor(WHITE);
  }

  uint8_t type() { return LCD_I2C; }
  void noBlink() { /*no support*/ }
  void blink() { /*no support*/ }
  void setCursor(uint8_t col, int8_t row) {
    /* assume 4 lines, the middle two lines
                     are row 0 and 1 */
    cy = (row + 1) * fontHeight;
    cx = col * fontWidth;
  }
  void noBacklight() { /*no support*/ }
  void backlight() { /*no support*/ }
  size_t write(uint8_t c) {
    setColor(BLACK);
    fillRect(cx, cy, fontWidth, fontHeight);
    setColor(WHITE);

    if (c < NUM_CUSTOM_ICONS && custom_chars[c] != NULL) {
      drawXbm(cx, cy, fontWidth, fontHeight, (const byte *)custom_chars[c]);
    } else {
      drawString(cx, cy, String((char)c));
    }
    cx += fontWidth;
    if (auto_display) display();  // todo: not very efficient
    return 1;
  }
  size_t write(const char *s) {
    uint8_t nc = strlen(s);
    setColor(BLACK);
    fillRect(cx, cy, fontWidth * nc, fontHeight);
    setColor(WHITE);
    drawString(cx, cy, String(s));
    cx += fontWidth * nc;
    if (auto_display) display();  // todo: not very efficient
    return nc;
  }
  void createChar(byte idx, PGM_P ptr) {
    if (idx >= 0 && idx < NUM_CUSTOM_ICONS) custom_chars[idx] = ptr;
  }
  void setAutoDisplay(bool v) { auto_display = v; }

 private:
  bool auto_display = true;
  uint8_t cx, cy;
  uint8_t fontWidth, fontHeight;
  PGM_P custom_chars[NUM_CUSTOM_ICONS];
};

// #elif defined(OSPI)
#else
#include <stdint.h>
#include <string.h>
#include <wiringPiI2C.h>

#include "font.h"
#include "images.h"

#define LCD_STD 0  // Standard LCD
#define LCD_I2C 1

#define BLACK 0
#define WHITE 1

unsigned char init_command[] = {
#if 0
    // OLED_CMD_SET_HORI_ADDR_MODE
    0xAE, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0xA1, 0xC8,
    0xD5, 0x80, 0xDA, 0x12, 0x81, 0xFF,
    0xA4, 0xDB, 0x40, 0x20, 0x00, 0x00, 0x10, 0x8D,
    0x14, 0x2E, 0xA6, 0xAF
#endif

#if 1
    // OLED_CMD_SET_PAGE_ADDR_MODE
    0xAE, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0xA1, 0xC8, 0xD5,
    0x80, 0xDA, 0x12, 0x81, 0xFF, 0xA4, 0xDB, 0x40, 0x20,
    0x02, 0x00, 0x10, 0x8D, 0x14, 0x2E, 0xA6, 0xAF
#endif
};

// Header Values
#define JUMPTABLE_BYTES 4

#define JUMPTABLE_LSB 1
#define JUMPTABLE_SIZE 2
#define JUMPTABLE_WIDTH 3
#define JUMPTABLE_START 4

#define WIDTH_POS 0
#define HEIGHT_POS 1
#define FIRST_CHAR_POS 2
#define CHAR_NUM_POS 3

class SSD1306Display {
 public:
  SSD1306Display(uint8_t _addr, uint8_t _sda, uint8_t _scl) {
    cx = 0;
    cy = 0;
    for (uint8_t i = 0; i < NUM_CUSTOM_ICONS; i++) custom_chars[i] = 0;

    int b;
    for (b = 0; b < 1024; b++) {
      frame[b] = 0x00;
    }

    i2cd = wiringPiI2CSetup(_addr);
    int i;
    for (i = 0; i < sizeof(init_command); i++) {
      unsigned int control = 0x00;  // Co = 0, D/C = 0
      wiringPiI2CWriteReg8(i2cd, control, init_command[i]);
    }
  }

  void init() {}  // Dummy function to match ESP8266
  void begin() {
    flipScreenVertically();
    setFont(Monospaced_plain_13);
    fontWidth = 8;
    fontHeight = 16;
  }

  void setFont(const uint8_t *f) { font = (uint8_t *)f; }

  void flipScreenVertically() {
    unsigned char command[3] = {0x00, 0x20, 0x00};
    for (int i = 0; i < sizeof(command); i++) {
      wiringPiI2CWriteReg8(i2cd, 0x00, command[i]);
    }
  }

  void display() {
    unsigned char page_command[3];

    for (int _page = 0; _page < 8; _page++) {
      page_command[0] = 0x00;
      page_command[1] = 0x10;
      page_command[2] = 0xB0 + _page;

      for (int i = 0; i < sizeof(page_command); i++) {
        wiringPiI2CWriteReg8(i2cd, 0x00, page_command[i]);
      }

      for (int col = 0; col < 128; col++) {
        wiringPiI2CWriteReg8(i2cd, 0x40, frame[_page * 128 + col]);
      }
    }
  }

  void clear() {
    int b;
    for (b = 0; b < 1024; b++) {
      frame[b] = 0x00;
    }

    display();
  }

  void setColor(uint8_t color) { this->color = color; }

  void drawPixel(uint8_t x, uint8_t y) {
    if (x >= 128 || y >= 64) return;

    if (color == WHITE) {
      frame[x + (y / 8) * 128] |= 1 << (y % 8);
    } else {
      frame[x + (y / 8) * 128] &= ~(1 << (y % 8));
    }
  }

  void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (int _x = x; _x < x + w; _x++) {
      for (int _y = y; _y < y + h; _y++) {
        drawPixel(_x, _y);
      }
    }
  }

  void clear(int start, int end) {
    setColor(BLACK);
    fillRect(0, (start + 1) * fontHeight, 128, (end - start + 1) * fontHeight);
    setColor(WHITE);
  }

  void print(const char *s) {
    while (*s) {
      write(*s);
      s++;
    }
  }

  uint8_t type() { return LCD_I2C; }
  void noBlink() { /*no support*/ }
  void blink() { /*no support*/ }
  void setCursor(uint8_t col, int8_t row) {
    /* assume 4 lines, the middle two lines
                    are row 0 and 1 */
    cy = (row + 1) * fontHeight;
    cx = col * fontWidth;
  }
  void noBacklight() { /*no support*/ }
  void backlight() { /*no support*/ }
  void drawXbm(int x, int y, int w, int h, const char *data) {
    for (int i = 0; i < w; i++) {
      for (int j = 0; j < h; j++) {
        if (data[i + (j / 8) * w] & (1 << (j % 8))) {
          drawPixel(x + i, y + j);
        }
      }
    }
  }

  void drawChar(int x, int y, char c) {
    uint8_t textHeight = font[HEIGHT_POS];
    uint8_t firstChar = font[FIRST_CHAR_POS];
    uint8_t numChars = font[CHAR_NUM_POS];
    uint16_t sizeOfJumpTable = numChars * JUMPTABLE_BYTES;

    if (c < firstChar || c >= firstChar + numChars) return;

    // 4 Bytes per char code
    uint8_t charCode = c - firstChar;

    uint8_t msbJumpToChar =
        font[JUMPTABLE_START +
             charCode * JUMPTABLE_BYTES];  // MSB \ JumpAddress
    uint8_t lsbJumpToChar = font[JUMPTABLE_START + charCode * JUMPTABLE_BYTES +
                                 JUMPTABLE_LSB];  // LSB /
    uint8_t charByteSize = font[JUMPTABLE_START + charCode * JUMPTABLE_BYTES +
                                JUMPTABLE_SIZE];  // Size
    uint8_t currentCharWidth =
        font[JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES +
             JUMPTABLE_WIDTH];  // Width

    // Test if the char is drawable
    if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
      // Get the position of the char data
      uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable +
                                  ((msbJumpToChar << 8) + lsbJumpToChar);
      int _y = y;
      int _x = x;

      setColor(WHITE);

      for (int b = 0; b < charByteSize; b++) {
        for (int i = 0; i < 8; i++) {
          if (font[charDataPosition + b] & (1 << i)) {
            drawPixel(_x, _y);
          }

          _y++;
          if (_y >= y + textHeight) {
            _y = y;
            _x++;
            break;
          }
        }
      }
    }
  }

  void drawString(int x, int y, const char *text) {
    int _x = x;
    int _y = y;

    while (*text) {
      if (*text == '\n') {
        _x = x;
        _y += fontHeight;
      } else {
        drawChar(_x, _y, *text);
        _x += fontWidth;
      }

      text++;
    }
  }

  uint8_t write(uint8_t c) {
    setColor(BLACK);
    fillRect(cx, cy, fontWidth, fontHeight);
    setColor(WHITE);
    char cc[2] = {(char)c, 0};

    if (c < NUM_CUSTOM_ICONS && custom_chars[c] != 0) {
      drawXbm(cx, cy, fontWidth, fontHeight, (const char *)custom_chars[c]);
    } else {
      drawString(cx, cy, cc);
    }
    cx += fontWidth;
    if (auto_display) display();  // todo: not very efficient
    return 1;
  }

  uint8_t write(const char *s) {
    uint8_t nc = strlen(s);
    setColor(BLACK);
    fillRect(cx, cy, fontWidth * nc, fontHeight);
    setColor(WHITE);
    drawString(cx, cy, s);
    cx += fontWidth * nc;
    if (auto_display) display();  // todo: not very efficient
    return nc;
  }

  void createChar(uint8_t idx, const char *ptr) {
    if (idx >= 0 && idx < NUM_CUSTOM_ICONS) custom_chars[idx] = ptr;
  }
  void setAutoDisplay(bool v) { auto_display = v; }

 private:
  bool auto_display = false;
  uint8_t cx, cy = 0;
  uint8_t fontWidth, fontHeight;
  const char *custom_chars[NUM_CUSTOM_ICONS];
  uint8_t frame[1024];
  int i2cd;
  bool color;
  uint8_t *font;
};
#endif

#endif  // SSD1306_DISPLAY_H
