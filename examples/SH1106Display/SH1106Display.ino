#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include <IndurUIIcons.h>

// For a 128x64 SH1106/SH1106G module, commonly sold as a 1.3-inch OLED.
// Check the controller, address and wiring of your module before uploading.
#ifndef INDURUI_SH1106_TWO_BUTTONS
#define INDURUI_SH1106_TWO_BUTTONS 0
#endif

constexpr uint8_t OLED_ADDRESS = 0x3C; // Change to 0x3D if your module uses it.
#if defined(ARDUINO_ARCH_ESP32)
// ESP32-S3 mini wiring; choose suitable free pins for a different board.
constexpr uint8_t OLED_SDA = 4, OLED_SCL = 3;
constexpr uint8_t BTN_LEFT = 12, BTN_RIGHT = 10, BTN_CENTER = 11;
#else
// Mega 2560: SDA=20, SCL=21. Each button connects its GPIO to GND.
constexpr uint8_t BTN_LEFT = 2, BTN_RIGHT = 3, BTN_CENTER = 4;
#endif

Adafruit_SH1106G display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SH1106G> canvas(display);
indur::UI ui(canvas, "SH1106 UI");
indur::Buttons buttons(ui, BTN_LEFT, BTN_RIGHT, INDURUI_SH1106_TWO_BUTTONS ? -1 : BTN_CENTER);

bool ready = false, running = false;
int speed = 60, contrast = 180;
uint16_t readings[3] = {};
uint32_t uptime = 0, lastSample = 0;
const char *sensorNames[] = {"L", "F", "R"};

void applySettings(indur::Id, indur::Change, void *) {
    display.setContrast(static_cast<uint8_t>(contrast));
}

void toggleRun(void *) {
    running = !running;
    ui.notify(running ? "DEMO RUNNING" : "DEMO STOPPED");
    // Bind your application here. This demonstration does not drive motors.
}

void setup() {
    Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
    Wire.begin(OLED_SDA, OLED_SCL);
#else
    Wire.begin();
#endif
    // SH110X takes (address, reset), unlike SSD1306's power-mode-first begin().
    if (!display.begin(OLED_ADDRESS, true)) {
        Serial.println("SH1106 initialization failed: check address, wiring and memory");
        return;
    }

    ui.setInputStyle(indur::InputStyle::Original);
    ui.addStatus("Up", uptime);
    ui.addStatus("SP", speed);
    for (uint8_t i = 0; i < 3; ++i) {
        ui.addBar(sensorNames[i], readings[i], 0, 1023);
        ui.addContext(sensorNames[i], readings[i]);
    }
    ui.addNumber("speed", "Demo speed", speed, 0, 100, 5, "%");
    ui.addToggle("running", "Demo enabled", running);
    ui.addNumber("contrast", "OLED contrast", contrast, 0, 255, 5);
    ui.addSettings("Settings", indur::AllGroups, indur::Root, indur::icons::settings());
    ui.addAction("Run / Stop", toggleRun, nullptr, indur::Root, indur::icons::run());
    ui.onChange(applySettings);
    buttons.begin();
    if (!ui.begin()) {
        Serial.println(ui.errorText());
        return;
    }
    applySettings(0, indur::Change::Loaded, nullptr);
    ready = true;
    ui.splash("SH1106 READY", indur::icons::robot(), 800);
}

void loop() {
    if (!ready)
        return;
    uint32_t now = millis();
    uptime = now / 1000;
    if (uint32_t(now - lastSample) >= 50) {
        lastSample = now;
        // Synthetic readings make the example run without external sensors.
        // Replace these assignments with your project's sensor values.
        for (uint8_t i = 0; i < 3; ++i) {
            uint16_t phase = (now / 4 + i * 341UL) % 2046;
            readings[i] = phase <= 1023 ? phase : 2046 - phase;
        }
    }
    buttons.update(now);
    ui.tick(now);
}
