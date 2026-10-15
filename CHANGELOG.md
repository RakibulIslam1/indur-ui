# Changelog

## 1.2.0 — 2026-10-15

- Restored the original instrument hierarchy: status strips, separate sensor bars, numbered three-row settings, live editor telemetry, and an animated loading splash. Cards remains a selectable style.
- Added body groups, menu-accessible adaptive dashboards, Auto/Cards/Bars/Arc layouts, independently paged status, and settings-group editor context.
- Added selectable Original/Standard controls and six labeled home-button bindings. Original editing uses left-increase/right-decrease and held Back/Save with either button count; context-aware full-screen hold feedback matches the original interaction.
- Preserved menu/setting selection across return paths, added custom engineering-value formatters and persistent dashboard alerts, and retained typed settings, journal storage, and custom screens.
- Reorganized MiniRobotTest with live sensor tuning, focused diagnostics, neutral motor outputs, and isolated NVS. Updated RobotDashboard and twelve exact OLED gallery captures.
- Added geometry, content grouping, editor context filtering, physical-button, and navigation history regression coverage.

## 1.1.0 — 2026-10-15

- Added a complete ESP32-S3 MiniRobotTest example and two/three-button PlatformIO environments: live IR/encoder data, custom diagnostic pages, grouped settings, lock behavior, gallery, and asynchronous progress.
- Kept mini motor outputs at neutral; persisted demo settings in a dedicated Preferences namespace without opening the robot's EEPROM storage.
- Added exact SSD1306 framebuffer export over serial and a browser OLED Lab with eight real-renderer captures, JSON import, Web Serial capture, and sharp PNG exports.
- Added browser pixel editing, mirror, rotate, and undo to Bitmap Studio; its menu preview uses a real captured background with the user's icon composited into the slot.
- Added reproducible Adafruit GFX capture generation, framebuffer/pixel-transform tests, browser import/export and simulated USB checks, and clean-package Arduino specification validation.
- Prepared Arduino Library Manager submission under Rakibul Islam's existing GitHub account and public maintainer contact.

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
