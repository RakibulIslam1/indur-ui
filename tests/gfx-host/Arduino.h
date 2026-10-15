#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#define ARDUINO 10819
#define PROGMEM
inline double radians(double degrees) {
    return degrees * 0.017453292519943295;
}
#ifdef _MSC_VER
inline uint16_t __builtin_bswap16(uint16_t value) {
    return uint16_t((value >> 8) | (value << 8));
}
#endif
class __FlashStringHelper;
class String {
  public:
    String(const char *text = "") : text_(text) {}
    size_t length() const { return text_.length(); }
    const char *c_str() const { return text_.c_str(); }

  private:
    std::string text_;
};
