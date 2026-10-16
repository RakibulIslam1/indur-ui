# OLED displays: SSD1306 and SH1106

IndurUI 1.3 adds an explicit SH1106/SH1106G setup using **Adafruit SH110X**, alongside the existing SSD1306 setup. Both use `IndurUIGFX.h` and the same menus, automatic layouts, icons, settings, storage, and two/three-button controls.

Choose the driver by the **controller and pixel resolution** of your module. A 1.3-inch diagonal alone does not identify its controller. The new example targets a **128×64 I²C SH1106/SH1106G** module; it does not automatically detect a display.

| Module controller | Install in Arduino Library Manager | Header | Display class | Initialization after starting Wire |
| --- | --- | --- | --- | --- |
| SSD1306, 128×64 | Adafruit SSD1306 | `Adafruit_SSD1306.h` | `Adafruit_SSD1306` | `display.begin(SSD1306_SWITCHCAPVCC, 0x3C)` |
| SH1106 / SH1106G, 128×64 | Adafruit SH110X | `Adafruit_SH110X.h` | `Adafruit_SH1106G` | `display.begin(0x3C, true)` |

Both also require **Adafruit GFX Library** and **Adafruit BusIO**. Accept dependency installation when installing IndurUI. Adafruit's [SH110X project](https://github.com/adafruit/Adafruit_SH110X) documents its controller support and initialization APIs.

## Run the 1.3-inch SH1106 example

1. Install **IndurUI** and its dependencies in Arduino Library Manager, or install the latest [release ZIP](https://github.com/RakibulIslam1/indur-ui/releases/latest).
2. Open **File → Examples → IndurUI → SH1106Display**.
3. Configure `OLED_ADDRESS` for your module (`0x3C` in the example; some modules use `0x3D`). An I²C scanner finds an address, but cannot identify the controller.
4. Configure the pins. The ESP32 example uses SDA4/SCL3 and left/right/center buttons 12/10/11, matching the mini setup. On Mega 2560 it uses hardware SDA20/SCL21 and buttons 2/3/4. Connect each active-low button to GND. Check your module's supply and logic requirements.
5. For two buttons, set `INDURUI_SH1106_TWO_BUTTONS` to `1` near the top of the sketch. Its default is three buttons.
6. Upload. Three animated sample readings run without sensors. Open Settings to edit demo speed, enabled state, and OLED contrast. No motor outputs are configured by this example.

The example selects Original controls: left/right taps navigate, editor taps increase/decrease, hold-left and release cancels/back, and hold-right and release selects/saves. With three buttons, center tap selects/saves and center hold cancels/back. Values in this example live in RAM; add a [storage adapter](STORAGE.md) for persistence.

From this repository, compile with:

```sh
pio run -e esp32s3-sh1106 -e mega-sh1106
```

## Switch an existing sketch

Keep your UI registrations and button/storage code. Replace the display include, type, and initialization:

```cpp
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>

Adafruit_SH1106G display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SH1106G> canvas(display);
indur::UI ui(canvas, "My Robot");

// In setup(), after configuring your bus:
// Wire.begin();                 // Mega: hardware I2C pins
// Wire.begin(sdaPin, sclPin);   // ESP32: your chosen pins
// if (!display.begin(0x3C, true)) { /* Handle initialization failure. */ }
// Register your widgets/settings, then check ui.begin().
```

SH110X's `begin()` takes **address, reset**. Passing `SSD1306_SWITCHCAPVCC` as its first argument gives the wrong address. SH1106 column addressing is handled by its driver; keep the canvas at **128×64**, with no manual x-offset or wider virtual screen.

Replace SSD1306-specific contrast commands with `display.setContrast(value)` for SH110X (`value` is 0–255). Other raw controller commands also need their matching driver API. `GfxCanvas` only needs monochrome GFX drawing and `display()`; it does not send SSD1306 commands.

For PlatformIO, select the new release and the correct driver:

```ini
lib_deps =
    https://github.com/RakibulIslam1/indur-ui.git#v1.3.0
    adafruit/Adafruit SH110X@^2.1.15
```

The library metadata installs the example drivers; your sketch includes the one it uses. IndurUI does not allocate a second display buffer.

## Mini robot test with SH1106

In a standalone library checkout, use **`mini_ui_test_sh1106`** for three buttons, or **`mini_ui_test_sh1106_2buttons`** for two:

```sh
pio run -e mini_ui_test_sh1106
pio run -e mini_ui_test_sh1106 -t upload
```

In Arduino IDE's **MiniRobotTest**, set `INDURUI_MINI_SH1106` to `1` near the top. Set `INDURUI_MINI_TWO_BUTTONS` separately. The existing `mini_ui_test` and `mini_ui_test_2buttons` environments continue to use SSD1306.

SH1106 mode retains live IR/encoder data, contrast control, grouped settings, neutral motor signals, isolated demo storage, and [OLED Lab USB capture](MINI_ROBOT_TEST.md#serial-controls-and-exact-oled-captures). At rotation 0, both Adafruit drivers expose the same vertical-page framebuffer format. The capture keeps the existing `indurui-ssd1306-v1` identifier for compatibility; it describes byte packing, not controller detection. The mini's custom I²C pins are configured before display initialization; the tested Arduino-ESP32 2.0.17 core retains an already started bus.

## Memory and verification

A monochrome 128×64 framebuffer needs **1,024 bytes of SRAM** with either driver, in addition to UI metadata, driver objects, application state, and stack. The classic Nano/Uno memory restriction remains; SH1106 support does not add a small-AVR mode. See [MCU compatibility](COMPATIBILITY.md).

CI compiles the SH1106 example on ESP32-S3 and Mega 2560 and both SH1106 mini button variants on ESP32-S3 with **Adafruit SH110X 2.1.15**. SSD1306 examples remain in the build matrix. These are compile checks; physical SH1106 display behavior has not been verified on hardware for this release. Other controllers, resolutions, rotations, and SPI configurations require their own driver setup and validation.
