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

## Wiring used by this example

| Component | Pins / configuration |
| --- | --- |
| SSD1306 OLED | 128×64, I²C address `0x3C`, SDA GPIO4, SCL GPIO3 |
| Left AS5600 | Same `Wire` bus, address `0x36` |
| Right AS5600 | `Wire1`, SDA GPIO2, SCL GPIO1, address `0x36` |
| Left / center / right buttons | GPIO12 / GPIO11 / GPIO10, active low, internal pull-ups |
| Left / front / right IR | GPIO5 / GPIO6 / GPIO7, 12-bit raw ADC |
| Left / right motor signal | GPIO9 / GPIO8, fixed 1500 µs neutral pulses at 50 Hz |
| Serial | USB CDC, 115200 baud |

The motor signals stay at the mini robot's configured neutral pulse throughout the test. Speed and gain settings are demonstration values and never drive the motors. Put the robot on a stand for the first upload to check your hardware's neutral behavior. Encoders are read only; turn the wheels by hand. Missing encoders show `-1` on the dashboard and “not responding” on their custom page.

## Button controls

Gestures take effect after release; the default hold threshold is 350 ms. Release when the hint appears, before the five-second stuck-button limit.

| Action | Three buttons | Two buttons |
| --- | --- | --- |
| Previous page / item, decrease | Tap left | Tap left |
| Next page / item, increase | Tap right | Tap right |
| Open menu / select / accept edit | Tap center | Hold right, release |
| Back / cancel edit | Hold center or left, release | Hold left, release |
| Repeat setting changes | Hold left/right while editing | Repeated taps; holds remain navigation gestures |

Dashboard left/right gestures change automatically calculated pages. The button-test page consumes navigation events to display the last received gesture; Back leaves it normally. Its three boxes also show raw button levels, including the center input in the two-button build.

## A useful test sequence

1. Wait for the splash to finish. Move an object near each IR sensor and check its bar. Change pages to see raw encoder angles, uptime, free heap, and status.
2. Open **Button test**. Check each tap and hold; leave with Back.
3. Open **Live sensors** and **Encoder angles**. Readings update without blocking navigation.
4. In **Settings → Demo controls**, edit speed and gain. Cancel once to verify rollback, then accept another edit. Turn **Lock speed/action** on: speed becomes read-only and the sample action in the gallery is disabled.
5. In **Settings → Display**, switch between icon/list menus and three/five cards. Adjust contrast, then restore it to a visible value.
6. In **Settings → Sampling / debug**, select the demo wave, adjust the sample interval, and enable serial logging. Return to real IR afterward.
7. In **UI gallery**, try notification, splash, and confirmation/progress. Confirm the progress demo: it runs for three seconds without motion. Cancel another confirmation to check the back path.
8. Change a setting, reboot, and check it survives. Use **Demo storage → Reload demo save** and **Reset demo only** to exercise load/defaults.

Accepted edits autosave to a dedicated ESP32 Preferences namespace, **`indurui-demo`**, key **`journal`**. The sketch never opens, writes, or erases the original firmware's EEPROM namespace. The old calibration and maze data remain available when you upload `mini_bot` again. The default partition layout must stay the same; erasing all flash would also erase saved robot data.

## Serial controls and exact OLED captures

| Character | Effect |
| --- | --- |
| `n` / `b` | Next / previous |
| `s` / `x` | Select / back |
| `0` / `h` | Dashboard / command help |
| `p` | Print the current 128×64 framebuffer |

Open the [OLED Lab](https://rakibulislam1.github.io/indur-ui/#oled-lab) in desktop Chrome or Edge. Close your serial monitor, click **Connect robot USB**, choose the board, and click **Capture OLED**. Download a native-size or sharply enlarged PNG. The browser sends only `p` followed by a newline.

Alternatively, send `p` in your serial monitor, copy the complete `INDURUI_FRAME {...}` line, and paste it in the lab. A saved JSON file works too. Frame data uses SSD1306 vertical eight-pixel pages: byte index `x + (y / 8) * width`, bit `y % 8`. This differs from the row-padded MSB-first format used for menu icons.

The website gallery is generated by the real library and Adafruit GFX with fixed sample values. A USB capture comes from the robot's actual display buffer. Neither is a photograph, and neither verifies electrical display behavior.

## Troubleshooting

- Blank OLED: check 3.3 V logic compatibility, address, SDA4/SCL3, and serial initialization messages. This example deliberately preserves its chosen I²C pins.
- Buttons act reversed: check the GPIO numbers and active-low wiring. Connect each switch between its GPIO and GND.
- USB unavailable: enable USB CDC on boot, use a data cable, and close programs already holding the port. JSON import works without Web Serial.
- Storage warning: use **Reset demo only** if you want to replace an invalid demo save. Do not erase the board's entire flash to fix a demo setting.
- Restore normal operation: upload the original **mini_bot** environment.

Both variants are compile-tested on the ESP32-S3 Arduino 2.0.17 toolchain. Physical OLED/button behavior must be checked on your robot. The example also includes the Arduino-ESP32 3.x LEDC API path, but that toolchain has not been validated by this release.
