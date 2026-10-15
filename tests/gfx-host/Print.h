#pragma once
#include <stddef.h>
#include <stdint.h>
class Print {
  public:
    virtual ~Print() {}
    virtual size_t write(uint8_t value) = 0;
    size_t write(const char *text) {
        size_t count = 0;
        while (text && *text)
            count += write((uint8_t)*text++);
        return count;
    }
    size_t print(const char *text) { return write(text); }
};
