#pragma once
#include <stdint.h>
#define INPUT 0
#define INPUT_PULLUP 2
extern int testPins[256];
extern int testModes[256];
inline int digitalRead(int pin) {
    return testPins[pin];
}
inline void pinMode(int pin, int mode) {
    testModes[pin] = mode;
}
