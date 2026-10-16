# MCU compatibility and memory budget

IndurUI's full interface needs room for its program, OLED framebuffer, UI metadata, application variables, and stack. The library is not a small-AVR UI. Version 1.0.1 narrows the package metadata and rejects unsuitable Arduino targets during compilation.

## Current support

| MCU / board | Status | Meaning |
| --- | --- | --- |
| ESP32-S3, tested on the ESP32-S3-DevKitC-1 target | Supported build baseline | All six generic examples compile in CI; adjust pins and display wiring for your board |
| ATmega2560 / Arduino Mega 2560 | Supported build baseline | All six generic examples compile in CI; 8 KiB SRAM still requires a careful application budget |
| Other ESP32 Arduino targets | Accepted family, not individually verified | Enough nominal memory does not establish board/core/display integration; validate the complete application |
| Other classic AVR devices with at least 8 KiB SRAM and 64 KiB flash | Pass the memory gate, not individually verified | Minimum eligibility only; not a promise that every application fits |
| Classic Nano / Uno / Pro Mini with ATmega328P, or ATmega168 variants | Unsupported; compilation rejected | Too little memory for this full UI configuration |
| ATmega32U4 boards such as Leonardo/Micro | Unsupported; compilation rejected | Below the AVR SRAM baseline |
| Nano Every and other megaAVR, SAMD, RP2040, STM32, ESP8266, and other Arduino architectures | No validated port in this release; compilation rejected | Some have capable hardware, but this release does not claim software support for them |

Supported build baselines mean successful automated compilation. The project does not claim physical validation of every OLED module, button arrangement, or storage device.

The package declares `architectures=esp32,avr`, and PlatformIO declares `espressif32` and `atmelavr`. Arduino architecture metadata cannot distinguish Nano from Mega, so `IndurUIBoard.h` also checks actual AVR device memory using the compiler's device headers. The 8 KiB/64 KiB limits are conservative project eligibility rules, not an automatic runtime memory estimator. There is no override that advertises a small-AVR mode.

## Classic Nano versus Nano ESP32

The **classic Nano** has 2 KiB SRAM and 32 KiB flash. This full library does not support it. [Arduino's classic Nano specifications](https://store.arduino.cc/products/arduino-nano)

The **Nano ESP32** uses an ESP32-S3 and is a different hardware platform. It belongs to the accepted ESP32 family; its exact board configuration is not separately built by this repository's CI. Select the correct Arduino board and configure its pin numbering, I²C pins, supply/logic compatibility, and display driver. Do not apply the classic Nano restriction to every product with Nano in its name. [Arduino Nano ESP32 manual](https://docs.arduino.cc/tutorials/nano-esp32/cheat-sheet/)

The **Mega 2560** provides 8 KiB SRAM and 256 KiB flash, making it the tested AVR baseline. Your robot code must still fit alongside the UI. [Arduino Mega specifications](https://store.arduino.cc/products/arduino-mega-2560-rev3)

## Flash, RAM, and EEPROM are different budgets

| Resource | What uses it |
| --- | --- |
| Flash / program storage | Firmware, display font, constant bitmap data; some ordinary AVR constants may also have SRAM copies |
| SRAM / heap / stack | Display framebuffer, UI objects/metadata, label strings on AVR, live variables, temporary buffers, application tasks |
| EEPROM or emulated settings storage | Two journal banks in the application-selected region |

Flash-backed AVR icons declared with `INDURUI_PROGMEM` consume flash rather than a full SRAM bitmap copy. Reducing icon count saves flash, but does not remove the OLED framebuffer or UI metadata. The converter runs on the computer, not on the MCU.

For the supplied Adafruit SSD1306 backend:

```text
framebuffer bytes = width × ceil(height / 8)
128×64 = 1024 bytes SRAM
128×32 =  512 bytes SRAM
```

The 128×64 framebuffer alone uses half of a classic Nano's 2 KiB SRAM. UI metadata, settings, serial/Wire buffers, robot code, and the stack require additional room. A smaller screen or a few smaller icons does not make this full library a supported classic Nano build. The allocation formula comes from the [Adafruit SSD1306 implementation](https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp).

The SH1106 example uses **Adafruit SH110X** and the same 1,024-byte framebuffer budget at 128×64. Choosing a 1.3-inch SH1106 display does not change the supported MCU policy. See [display setup and verification](DISPLAYS.md).

Bitmap payloads use `ceil(width / 8) × height` bytes:

| Icon | Bytes per bitmap |
| --- | ---: |
| 16×16 | 32 |
| 24×24 | 72 |
| 32×32 | 128 |
| 64×64 | 512 |

Changing `theme.iconSize` only changes rendering size. It does not shrink a larger stored bitmap; export a smaller bitmap with Bitmap Studio to reduce its data size.

## Budget a real application

1. Start from the smaller TwoButtons example on a supported board, then add features gradually.
2. Check `display.begin()`, each relevant registration result, and `ui.begin()`; allocation failures must be handled.
3. Include runtime allocations in your budget. A compile report's static-RAM number does **not** include the SSD1306 framebuffer, UI metadata allocated during setup, or temporary persistence buffers.
4. Registration grows metadata arrays; peak setup allocation includes the old and new array. Register only the items you need.
5. Rendering uses fixed local buffers. Saving needs a payload snapshot; loading needs roughly 9 bytes per registered setting; default reset needs roughly 8 bytes per setting. These are in addition to metadata, stack, and backend allocations.
6. Leave stack/interrupt/task headroom for the actual robot. More pages make data fit on the display; they do not make unlimited registrations fit in RAM.
7. Test the full application on the actual board, including menus, edits, save/load, and maximum application workload.

For modest AVR projects, use few settings, short labels, and 16×16 or 32×32 flash-backed icons. For large sensor dashboards, more settings, networking, and a substantial robot application, an ESP32-S3 offers more room. Neither option removes the need for a complete memory budget.

A separate fixed-capacity, small-display UI could target classic Nano hardware in the future. This release does not include such a mode.
