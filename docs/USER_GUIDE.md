# IndurUI user guide

IndurUI displays your project's data and lets buttons control menus and edit settings. Your application continues to own its sensors, motors, control loop, and variables.

## 1. Choose a supported board

Start with an **ESP32-S3** or **Arduino Mega 2560**, the two build-tested targets. The classic ATmega328P Nano/Uno is unsupported and is rejected at compile time. Other boards are described in the [compatibility and memory guide](COMPATIBILITY.md). There is no automatic small-MCU mode.

Use an SSD1306 128×64 I²C display for the supplied examples. The library can use other compatible displays through an adapter, but the example driver and wiring must match your actual module.

## 2. Install the library

1. Download **IndurUI-1.0.1.zip** from [Releases](https://github.com/RakibulIslam1/indur-ui/releases/latest).
2. In Arduino IDE, choose **Sketch → Include Library → Add .ZIP Library…**.
3. Install **Adafruit GFX Library**, **Adafruit SSD1306**, and their **Adafruit BusIO** dependency through Library Manager.
4. Select the correct board/core and port.
5. Open **File → Examples → IndurUI → QuickStart** or **TwoButtons**.

The Bitmap Studio desktop converter is optional and runs separately on your computer.

## 3. Connect and configure the display and buttons

Connect OLED SDA/SCL to your board's I²C pins, and use supply and logic levels supported by both devices. Configure `Wire.begin(...)`, the display dimensions, and its I²C address; the examples use `0x3C`.

Each active-low button connects its chosen GPIO to GND. Internal pull-ups are enabled by default. The examples' pins 2/3/4 are placeholders; choose suitable free pins for your board.

```cpp
indur::Buttons buttons(ui, leftPin, rightPin);            // two buttons
// Or use this constructor instead:
indur::Buttons buttons(ui, leftPin, rightPin, centerPin); // three buttons
```

Use one constructor, call `buttons.begin()` in setup, and call `buttons.update(now)` in loop.

## 4. Register data for the home dashboard

Keep the variables alive for as long as the UI uses them. Global variables or long-lived object members are suitable.

```cpp
float battery = 7.8f;
int distance = 250;
bool running = false;

// In setup, before ui.begin():
ui.addValue("Battery", battery, "V", 2);
ui.addBar("Distance", distance, 0, 1000, "mm");
ui.addValue("Running", running);
```

The dashboard rereads the variables when rendering. Update them normally in your project; the dashboard does not modify them. Use numeric callbacks for calculated readings and text callbacks for strings or custom objects.

The library lays out cards and adds pages as needed. Default 128×64 layout shows two large cards per page. For compact sensor bars:

```cpp
indur::Theme theme;
theme.minWidgetWidth = 24;
ui.setTheme(theme);
```

This allows five narrow cards across a 128-pixel display. Long text shortens, and a number that cannot fit displays `--` instead of misleading partial digits. Use wider cards when full numeric readings matter. Every registered item consumes metadata RAM, including items on other pages.

## 5. Add editable settings

```cpp
int speed = 80;
float gain = 1.2f;
bool logging = false;
uint8_t mode = 0;
const char *modes[] = {"Explore", "Manual"};

// In setup:
ui.addNumber("drive.speed", "Speed", speed, 0, 100, 5, "%");
ui.addNumber("drive.gain", "Gain", gain, 0, 10, 0.1, "", 1);
ui.addToggle("logging", "Logging", logging);
ui.addChoice("mode", "Mode", mode, modes, 2);
```

Keys must be unique and stable. Initial values and bounds must fit the variable's type. Use an integer variable for whole-number steps and a floating type for fractional steps.

Entering the editor creates a pending value. Save applies it; Cancel preserves the existing project variable. Settings appear in the menu automatically unless you register explicit settings entries. Your application can use `onChange(...)` to update derived configuration after an accepted change.

## 6. Navigate

| Operation | Two buttons | Three buttons |
| --- | --- | --- |
| Previous / next item or dashboard page | Tap left / right | Tap left / right |
| Decrease / increase editor value | Tap left / right | Tap left / right; hold to repeat |
| Enter menu, select item, or save edit | Hold right, then release | Tap center |
| Back or cancel edit | Hold left, then release | Hold center, then release |

Outside the three-button editor, holding left/right also gives Back/Select. Default hold threshold is 350 ms, measured after debounce. The release-action hint appears while a navigation hold is active. A five-second held/stuck input is suppressed until release; timing is configurable.

## 7. Add actions, submenus, and custom pages

```cpp
void requestRun(void *) {
    runRequested = true; // Let your ordinary application loop handle the job.
    ui.notify("RUN REQUESTED");
}

// In setup:
indur::Id tools = ui.addMenu("Tools");
ui.addAction("Run", requestRun, nullptr, tools);
ui.addSettings("Settings", indur::AllGroups, tools);
```

Callbacks should return promptly. Use `addScreen(...)` for application-specific diagnostics, graphs, or tuning panels. `confirm(...)`, `progress(...)`, `splash(...)`, and `notify(...)` provide reusable workflow screens. See [RobotDashboard](../examples/RobotDashboard/RobotDashboard.ino) for a complete nonblocking example.

## 8. Choose icons

```cpp
#include <IndurUIIcons.h>
// In setup:
ui.addAction("Run", requestRun, nullptr, indur::Root, indur::icons::run());
```

To create your own, open **[Bitmap Studio online](https://rakibulislam1.github.io/indur-ui/)**, choose an image, and export a header. Conversion happens locally in your browser.

For the offline desktop version, install the converter dependency and run:

```sh
python -m pip install -r tools/requirements.txt
python tools/bitmap_converter.py
```

Open an image, select 16×16 or 32×32, adjust the preview, and export a header. Include that header and call `ui.setIcon(menuId, exportedIcon)`. The [bitmap guide](BITMAPS.md) explains CLI use and memory requirements. Flash-backed icons avoid full bitmap SRAM copies on AVR. Bigger source images still consume more flash even when displayed at a smaller size.

Use `ui.setMenuStyle(indur::MenuStyle::List)` for a text list. Switching to list view changes rendering; it does not remove registered data or an already allocated framebuffer.

## 9. Save settings across restarts

Follow [PersistentSettings](../examples/PersistentSettings/PersistentSettings.ino). It shows the full order:

1. Initialize EEPROM when the board requires it.
2. Construct the storage adapter.
3. Register valid defaults and initialize the UI.
4. Attach a region large enough for `ui.requiredStorageBytes()`.
5. Load and check the result; save initial defaults only when appropriate.

Accepted edits autosave when storage is attached. Without storage, settings work in RAM. If application code changes a variable directly, call `save()` explicitly when you want it persisted. Do not save continuously in the control loop. See [storage details](STORAGE.md) for failure handling, schema changes, and flash-emulation limits.

## 10. Keep the application running

```cpp
void loop() {
    uint32_t now = millis();
    updateSensors();
    runApplicationTasks();
    buttons.update(now);
    ui.tick(now);
}
```

Use your own functions in place of the placeholders. No UI wait loop is required. Display transfers, EEPROM commits, and callbacks still consume time; keep timing-critical work in an appropriate independent context and provide coherent sensor snapshots.

Check `display.begin()` and `ui.begin()` and handle `ui.errorText()` on failure. A successful compile does not establish that your combined application has enough runtime memory.

## If something goes wrong

| Symptom | Check |
| --- | --- |
| Build rejects classic Nano/Uno | Expected restriction; use a supported board for this full library |
| Blank OLED | Address, I²C pins, supply/logic levels, dimensions, and display initialization result |
| Setup fails after adding items | Registration errors, duplicate keys, bounds, or insufficient memory |
| Number shows `--` | Increase card width or use fewer/narrower-format readings per page |
| Value resets after restart | Storage attachment, load result, explicit saves for application-originated changes |
| Edited value does not change immediately | It remains pending until Save |
| Menu pauses the application | Check your callbacks, display transfer time, and EEPROM commits for blocking work |

The [API reference](API.md) documents all methods and lifetime rules. The [compatibility guide](COMPATIBILITY.md) explains why display paging and flash icons do not remove MCU memory limits.
