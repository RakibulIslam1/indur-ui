# Mini robot UI test

`MiniRobotTest` is a complete interface test for the ESP32-S3 mini robot. It uses its existing OLED, three IR inputs, two AS5600 encoders, and buttons. It provides both three-button and two-button builds. This example is specific to this wiring; the other examples cover generic supported boards.

## Upload from the robot workspace

Choose **mini_ui_test**, then Build and Upload in PlatformIO. The original `mini_bot` environment remains available.

```powershell
pio run -e mini_ui_test
pio run -e mini_ui_test -t upload
pio device monitor -e mini_ui_test
```

For two buttons, replace the environment with `mini_ui_test_2buttons`. GPIO11 is ignored for navigation in this build. Close the serial monitor before uploading or connecting the website's USB capture tool.

From a standalone checkout of this library, the same commands work: its `platformio.ini` includes both environments. In Arduino IDE, open **File → Examples → IndurUI → MiniRobotTest**, select your ESP32-S3 board, enable USB CDC on boot, and use 4 MB flash with the default partition scheme. For two buttons, change `INDURUI_MINI_TWO_BUTTONS` to `1` near the top of the sketch.

**1.3-inch SH1106 option:** in the library checkout, select `mini_ui_test_sh1106` (three buttons) or `mini_ui_test_sh1106_2buttons` (two buttons). In Arduino IDE, set `INDURUI_MINI_SH1106` to `1` at the top of MiniRobotTest. These builds use **Adafruit SH110X** for a 128×64 SH1106/SH1106G module on the same SDA4/SCL3 wiring. Contrast, live data, settings, and USB framebuffer capture work through the selected driver. The original environments default to SSD1306. See the [display setup guide](DISPLAYS.md).

## Wiring used by this example

| Component | Pins / configuration |
| --- | --- |
| SSD1306 OLED, or SH1106 in its optional build | 128×64, I²C address `0x3C`, SDA GPIO4, SCL GPIO3 |
| Left AS5600 | Same `Wire` bus, address `0x36` |
| Right AS5600 | `Wire1`, SDA GPIO2, SCL GPIO1, address `0x36` |
| Left / center / right buttons | GPIO12 / GPIO11 / GPIO10, active low, internal pull-ups |
| Left / front / right IR | GPIO5 / GPIO6 / GPIO7, 12-bit raw ADC |
| Left / right motor signal | GPIO9 / GPIO8, fixed 1500 µs neutral pulses at 50 Hz |
| Serial | USB CDC, 115200 baud |

The motor signals stay at the mini robot's configured neutral pulse throughout the test. Speed and gain settings are demonstration values and never drive the motors. Put the robot on a stand for the first upload to check your hardware's neutral behavior. Encoders are read only; turn the wheels by hand. Missing encoders show `-1` on the dashboard and “not responding” on their custom page.

## Original-style controls

The mini example selects `InputStyle::Original`. Gestures take effect after release; the default hold threshold is 350 ms. The full-screen caption shows the pending action. Release before the five-second stuck-button limit.

| Home gesture | Action |
| --- | --- |
| Left tap / hold | Sensor arc / toggle a persistent demo alert |
| Center tap / hold | System data / all settings |
| Right tap / hold | Menu / confirmation and progress demo |

With two buttons, the center gestures are unavailable; System data and every settings group remain reachable in the menu. The footer shows tap actions in their physical left/center/right positions.

| Inside menus and settings | Both button counts |
| --- | --- |
| Tap left / right | Previous / next |
| Edit: tap left / right | Increase / decrease |
| Hold left, release | Back / discard edit |
| Hold right, release | Select / save edit |
| Center tap / hold, when present | Select/save / back/cancel |

Holding left/right inside the editor remains Back/Save, including the three-button build. The generic Standard profile remains available to other projects with conventional direction and three-button repeating edits.

Single-page grouped dashboards return on any tap, like the original diagnostic screens. Multi-page dashboards use left/right paging and a held Back. The button-test custom page consumes navigation events to show the last received gesture; Back leaves it. Its three boxes also show raw button levels.

## A useful test sequence

1. Wait for the loading splash. Home has the compact L / SP / R strip and three vertical IR bars. Move an object near each sensor.
2. Tap left for the staggered L / F / R readings. Tap again to return home.
3. Tap center for paged System data, or open it through **Diagnostics**. Check uptime, free heap, status, and raw angles. Hold left to return.
4. Open **Diagnostics / Button test** and check each tap/hold. Try **Encoder angles** from the root menu and turn the wheels by hand.
5. Open **Settings / Demo controls**. Its numbered list shows three two-line rows. The editor keeps six live status/context fields above the pending value. Cancel once, accept another edit, and check the selected row remains in place.
6. Turn **Lock speed/action** on: speed becomes read-only and the gallery's sample action is disabled.
7. In **Settings / Display**, switch icon/list menus and Original/Cards interface style. Contrast remains adjustable.
8. In **Settings / Sampling / debug**, select the demo wave, adjust sampling, and enable serial logging. In **Sensor tuning**, edit each demo threshold while watching all three live IR values in the editor. Thresholds demonstrate saved UI settings; they do not modify the original robot controller.
9. In **UI gallery**, try notification, loading splash, persistent alert, and confirmed progress. The progress demo runs for three seconds without motion. Back cancels its work. A held left on home toggles the same persistent demo alert.
10. Change a setting, reboot, and check it survives. Use **Demo storage / Reload demo save** and **Reset demo only**. Return from nested pages and confirm the previous menu item stays selected.

Accepted edits autosave to a dedicated ESP32 Preferences namespace, **`indurui-demo`**, key **`journal`**. The sketch never opens, writes, or erases the original firmware's EEPROM namespace. The old calibration and maze data remain available when you upload `mini_bot` again. The default partition layout must stay the same; erasing all flash would also erase saved robot data.

## Serial controls and exact OLED captures

| Character | Effect |
| --- | --- |
| `n` / `b` | Next / previous |
| `s` / `x` | Select / back |
| `0` / `h` | Dashboard / command help |
| `p` | Print the current 128×64 framebuffer |

Open the [OLED Lab](https://rakibulislam1.github.io/indur-ui/#oled-lab) in desktop Chrome or Edge. Close your serial monitor, click **Connect robot USB**, choose the board, and click **Capture OLED**. Download a native-size or sharply enlarged PNG. The browser sends only `p` followed by a newline.

Alternatively, send `p` in your serial monitor, copy the complete `INDURUI_FRAME {...}` line, and paste it in the lab. A saved JSON file works too. Frame data uses vertical eight-pixel pages: byte index `x + (y / 8) * width`, bit `y % 8`. Both drivers expose this format at rotation 0, so the SH1106 build keeps the existing `indurui-ssd1306-v1` identifier for compatibility. This differs from the row-padded MSB-first format used for menu icons.

The website gallery is generated by the real library and Adafruit GFX with fixed sample values. A USB capture comes from the robot's actual display buffer. Neither is a photograph, and neither verifies electrical display behavior.

## Troubleshooting

- Blank OLED: check 3.3 V logic compatibility, address, SDA4/SCL3, and serial initialization messages. This example deliberately preserves its chosen I²C pins.
- Buttons act reversed: check the GPIO numbers and active-low wiring. Connect each switch between its GPIO and GND.
- USB unavailable: enable USB CDC on boot, use a data cable, and close programs already holding the port. JSON import works without Web Serial.
- Storage warning: use **Reset demo only** if you want to replace an invalid demo save. Do not erase the board's entire flash to fix a demo setting.
- Restore normal operation: upload the original **mini_bot** environment.

All four driver/button variants are compile-tested on the ESP32-S3 Arduino 2.0.17 toolchain. Physical SH1106 OLED behavior has not been verified for this release and must be checked on your robot. The example also includes the Arduino-ESP32 3.x LEDC API path, but that toolchain has not been validated by this release.
