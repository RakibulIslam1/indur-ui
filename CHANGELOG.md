# Changelog

## 1.0.1 — 2026-10-15

- Added a step-by-step user guide and an MCU compatibility/memory guide.
- Narrowed Arduino/PlatformIO metadata to ESP32 and classic AVR families.
- Added a compile-time gate rejecting classic Nano/Uno and AVR devices below 8 KiB SRAM or 64 KiB flash, plus Arduino architectures without a validated port.
- Clarified build-tested targets versus unverified boards, flash-backed icons versus SRAM framebuffers, and classic Nano versus Nano ESP32.
- Added target-policy checks using real AVR device headers.

## 1.0.0 — 2026-10-15

- Initial standalone Arduino library based on the Indur robot's OLED interaction design.
- Automatic dashboard cards/pages for live numbers, bars, booleans, and text.
- Nested icon/list menus, settings groups, actions, and custom screens.
- Two/three-button navigation, hold feedback, repeating edits, raw gesture interception, and stuck detection.
- Typed pending-value editors with save/cancel and change callbacks.
- Configurable EEPROM/custom storage, two-bank CRC journal, defaults, and save-failure rollback.
- Bitmap Studio desktop/CLI converter, eight bundled icons, and custom icon example.
- Five Arduino examples, API/storage/converter documentation, graphics, native tests, and two-board compile matrix.
