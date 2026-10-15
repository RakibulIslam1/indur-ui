#pragma once

// This policy applies to Arduino firmware. The portable core also builds on a host.
#if defined(ARDUINO)
#if defined(ARDUINO_ARCH_AVR)
#include <avr/io.h>
#if !defined(RAMSTART) || !defined(RAMEND) || !defined(FLASHEND)
#error "IndurUI cannot verify this AVR's memory. See docs/COMPATIBILITY.md."
#elif (RAMEND - RAMSTART + 1) < 8192 || FLASHEND < 65535UL
#error                                                                                             \
    "IndurUI requires AVR >=8 KiB SRAM and >=64 KiB flash. Classic Nano/Uno are unsupported. Use ATmega2560 or ESP32; see docs/COMPATIBILITY.md."
#endif
#elif !defined(ARDUINO_ARCH_ESP32)
#error                                                                                             \
    "IndurUI currently targets ESP32 Arduino and large classic AVR. Other architectures need a validated port; see docs/COMPATIBILITY.md."
#endif
#endif
