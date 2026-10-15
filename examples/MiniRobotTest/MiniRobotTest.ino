#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <esp_arduino_version.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include <IndurUIIcons.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "MiniRobotTest requires the ESP32-S3 mini robot; use QuickStart on other supported MCUs."
#endif
#ifndef INDURUI_MINI_TWO_BUTTONS
#define INDURUI_MINI_TWO_BUTTONS 0
#endif

// Matches the mini robot's current config.h defines (not its older wiring comments).
constexpr uint8_t BTN_LEFT = 12, BTN_RIGHT = 10, BTN_CENTER = 11;
constexpr uint8_t OLED_SDA = 4, OLED_SCL = 3, ENC_R_SDA = 2, ENC_R_SCL = 1;
constexpr uint8_t LEFT_MOTOR_PIN = 9, RIGHT_MOTOR_PIN = 8;
constexpr uint8_t sensorPins[] = {5, 6, 7};
const char *sensorNames[] = {"Left IR", "Front IR", "Right IR"};

// A separate NVS namespace: never opens or writes the robot's EEPROM namespace.
class DemoStorage : public indur::Storage {
  public:
    bool begin() {
        memset(data_, 0xff, sizeof(data_));
        if (!preferences_.begin("indurui-demo", false))
            return false;
        if (preferences_.getBytesLength("journal") == sizeof(data_))
            return preferences_.getBytes("journal", data_, sizeof(data_)) == sizeof(data_);
        return true;
    }
    size_t capacity() const override { return sizeof(data_); }
    bool read(size_t address, uint8_t &value) override {
        if (address >= sizeof(data_))
            return false;
        value = data_[address];
        return true;
    }
    bool write(size_t address, uint8_t value) override {
        if (address >= sizeof(data_))
            return false;
        data_[address] = value;
        return true;
    }
    bool flush() override {
        return preferences_.putBytes("journal", data_, sizeof(data_)) == sizeof(data_);
    }

  private:
    Preferences preferences_;
    uint8_t data_[1024];
};

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "MINI UI LAB");
indur::Buttons buttons(ui, BTN_LEFT, BTN_RIGHT, INDURUI_MINI_TWO_BUTTONS ? -1 : BTN_CENTER);
DemoStorage storage;
bool ready = false, storageReady = false, jobRunning = false;
bool logging = false, locked = false;
int ir[3] = {}, angleLeft = -1, angleRight = -1;
uint32_t uptime = 0, heapBytes = 0, jobStart = 0, lastSample = 0;
int demoSpeed = 60, sampleMs = 50, contrast = 180;
float demoGain = 1.2f;
uint8_t menuStyle = 0, compact = 0, synthetic = 0;
const char *styleNames[] = {"Icons", "List"};
const char *layoutNames[] = {"3 cards", "5 cards"};
const char *sourceNames[] = {"Real IR", "Demo wave"};
indur::Id speedSetting = indur::Invalid, lockableAction = indur::Invalid;
const char *lastGesture = "Tap a button";

void neutralOutputs() {
    constexpr uint32_t duty = (1500UL * (1UL << 14) + 10000) / 20000;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(LEFT_MOTOR_PIN, 50, 14, 0);
    ledcAttachChannel(RIGHT_MOTOR_PIN, 50, 14, 1);
    ledcWrite(LEFT_MOTOR_PIN, duty);
    ledcWrite(RIGHT_MOTOR_PIN, duty);
#else
    ledcSetup(0, 50, 14);
    ledcSetup(1, 50, 14);
    ledcAttachPin(LEFT_MOTOR_PIN, 0);
    ledcAttachPin(RIGHT_MOTOR_PIN, 1);
    ledcWrite(0, duty);
    ledcWrite(1, duty);
#endif
    // This example never changes these 1500-us neutral pulses or commands movement.
}

int readAngle(TwoWire &bus) {
    bus.beginTransmission(0x36);
    bus.write(0x0c);
    if (bus.endTransmission(false) != 0 || bus.requestFrom(uint8_t(0x36), uint8_t(2)) != 2)
        return -1;
    return ((bus.read() << 8) | bus.read()) & 0x0fff;
}

void drawSensors(indur::Canvas &c, void *) {
    c.text(2, 1, synthetic ? "IR / DEMO WAVE" : "IR / LIVE ADC");
    const char *labels[] = {"L", "F", "R"};
    for (uint8_t i = 0; i < 3; ++i) {
        int x = 3 + i * 42;
        c.text(x, 15, labels[i]);
        char number[8];
        snprintf(number, sizeof(number), "%d", ir[i]);
        c.text(x, 27, number);
        c.rect(x, 39, 36, 5);
        c.fillRect(x + 1, 40, (ir[i] * 34L) / 4095, 3);
    }
    c.text(2, 55, INDURUI_MINI_TWO_BUTTONS ? "Hold left: back" : "Hold center: back");
}

void drawEncoders(indur::Canvas &c, void *) {
    c.text(2, 1, "AS5600 RAW ANGLES");
    char row[22];
    if (angleLeft < 0)
        snprintf(row, sizeof(row), "L: not responding");
    else
        snprintf(row, sizeof(row), "L: %4d / 4095", angleLeft);
    c.text(2, 16, row);
    if (angleRight < 0)
        snprintf(row, sizeof(row), "R: not responding");
    else
        snprintf(row, sizeof(row), "R: %4d / 4095", angleRight);
    c.text(2, 29, row);
    c.text(2, 43, "Turn wheels by hand");
    c.text(2, 55, "Hold left: back");
}

void drawButtons(indur::Canvas &c, void *) {
    c.text(2, 1, INDURUI_MINI_TWO_BUTTONS ? "BUTTON TEST / TWO" : "BUTTON TEST / THREE");
    const int pins[] = {BTN_LEFT, BTN_CENTER, BTN_RIGHT};
    const char *names[] = {"LEFT", "CENTER", "RIGHT"};
    for (uint8_t i = 0; i < 3; ++i) {
        int x = 1 + 43 * i;
        bool down = !digitalRead(pins[i]);
        c.roundRect(x, 16, 40, 21, 3, down);
        c.text(x + 2, 23, names[i], 1, !down);
    }
    c.text(2, 42, lastGesture);
    c.text(2, 55, "Hold left: back");
}

bool buttonEvents(indur::Event event, void *) {
    switch (event) {
    case indur::Event::Previous:
        lastGesture = "PREVIOUS received";
        break;
    case indur::Event::Next:
        lastGesture = "NEXT received";
        break;
    case indur::Event::Select:
        lastGesture = "SELECT received";
        break;
    case indur::Event::Back:
        lastGesture = "BACK received";
        return false;
    }
    return true;
}

void drawAbout(indur::Canvas &c, void *) {
    c.text(2, 1, "INDURUI / MINI TEST");
    c.text(2, 15, "ESP32-S3 / 128x64");
    c.text(2, 27, "Motors: neutral");
    c.text(2, 39, storageReady ? "NVS: indurui-demo" : "NVS: unavailable");
    c.text(2, 55, "USB: p = screenshot");
}

void goHome(void *) {
    ui.home();
}
void showNotification(void *) {
    ui.notify("HELLO FROM INDURUI", 1500);
}
void showSplash(void *) {
    ui.splash("MINI UI LAB", indur::icons::robot(), 1200);
}
void lockedSample(void *) {
    ui.notify("ACTION ALLOWED");
}
void runDemo(void *) {
    jobRunning = true;
    jobStart = millis();
    ui.progress("UI DEMO / NO MOTION", 0);
}
void askDemo(void *) {
    ui.confirm("Start UI demo?", runDemo);
}
void saveSettings(void *) {
    ui.notify(storageReady && ui.save() ? "DEMO SETTINGS SAVED" : "SAVE FAILED");
}
void loadSettings(void *) {
    ui.notify(storageReady && ui.load() == indur::LoadResult::Loaded ? "DEMO SETTINGS LOADED"
                                                                     : "NO VALID DEMO SAVE");
}
void restoreDefaults(void *) {
    ui.notify(ui.resetDefaults(storageReady) ? "DEMO DEFAULTS READY" : "RESET FAILED");
}
void askDefaults(void *) {
    ui.confirm("Reset DEMO settings?", restoreDefaults);
}

void applySettings(indur::Id, indur::Change, void *) {
    indur::Theme theme;
    theme.minWidgetWidth = compact ? 24 : 40;
    theme.refreshMs = 100;
    ui.setTheme(theme);
    ui.setMenuStyle(menuStyle ? indur::MenuStyle::List : indur::MenuStyle::Icons);
    ui.setReadOnly(speedSetting, locked);
    ui.setEnabled(lockableAction, !locked);
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(contrast);
}

void statusText(void *, char *out, size_t capacity) {
    snprintf(out, capacity, "%s", jobRunning ? "UI DEMO" : "MOTORS NEUTRAL");
}

void registerInterface() {
    for (uint8_t i = 0; i < 3; ++i)
        ui.addBar(sensorNames[i], ir[i], 0, 4095);
    ui.addValue("L angle", angleLeft, "raw", 0);
    ui.addValue("R angle", angleRight, "raw", 0);
    ui.addValue("Uptime", uptime, "s", 0);
    ui.addValue("Heap", heapBytes, "bytes", 0);
    ui.addText("Status", statusText);
    speedSetting = ui.addNumber("demo.speed", "Demo speed", demoSpeed, 0, 100, 5, "%", 0, 1);
    ui.addNumber("demo.gain", "Demo gain", demoGain, 0, 10, .1, "", 1, 1);
    ui.addToggle("demo.lock", "Lock speed/action", locked, 1);
    ui.addChoice("display.style", "Menu style", menuStyle, styleNames, 2, 2);
    ui.addChoice("display.layout", "Card layout", compact, layoutNames, 2, 2);
    ui.addNumber("display.contrast", "OLED contrast", contrast, 0, 255, 5, "", 0, 2);
    ui.addChoice("sensor.source", "Sensor source", synthetic, sourceNames, 2, 3);
    ui.addNumber("sensor.period", "Sample period", sampleMs, 20, 500, 10, "ms", 0, 3);
    ui.addToggle("debug.logging", "Serial logging", logging, 3);
    ui.addAction("Dashboard", goHome, nullptr, indur::Root, indur::icons::home());
    ui.addScreen("Live sensors", drawSensors, nullptr, nullptr, indur::Root,
                 indur::icons::sensors());
    ui.addScreen("Encoder angles", drawEncoders, nullptr, nullptr, indur::Root,
                 indur::icons::robot());
    ui.addScreen("Button test", drawButtons, buttonEvents, nullptr, indur::Root,
                 indur::icons::tools());
    auto settings = ui.addMenu("Settings", indur::Root, indur::icons::settings());
    ui.addSettings("Demo controls", 1, settings, indur::icons::run());
    ui.addSettings("Display", 2, settings, indur::icons::settings());
    ui.addSettings("Sampling / debug", 3, settings, indur::icons::sensors());
    auto gallery = ui.addMenu("UI gallery", indur::Root, indur::icons::info());
    ui.addAction("Notification", showNotification, nullptr, gallery, indur::icons::info());
    ui.addAction("Splash screen", showSplash, nullptr, gallery, indur::icons::robot());
    ui.addAction("Confirm / progress", askDemo, nullptr, gallery, indur::icons::run());
    lockableAction =
        ui.addAction("Lockable sample", lockedSample, nullptr, gallery, indur::icons::tools());
    auto saved = ui.addMenu("Demo storage", indur::Root, indur::icons::storage());
    ui.addAction("Save demo settings", saveSettings, nullptr, saved, indur::icons::storage());
    ui.addAction("Reload demo save", loadSettings, nullptr, saved, indur::icons::storage());
    ui.addAction("Reset demo only", askDefaults, nullptr, saved, indur::icons::tools());
    ui.addScreen("About / wiring", drawAbout, nullptr, nullptr, indur::Root, indur::icons::info());
    ui.onChange(applySettings);
}

void captureFrame() {
    // Raw SSD1306 page bytes, not the horizontal format used for icon arrays.
    ui.render();
    Serial.print(
        "INDURUI_FRAME {\"format\":\"indurui-ssd1306-v1\",\"width\":128,\"height\":64,\"data\":\"");
    const uint8_t *buffer = display.getBuffer();
    const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 1024; ++i) {
        char pair[] = {hex[buffer[i] >> 4], hex[buffer[i] & 15]};
        Serial.write((const uint8_t *)pair, 2);
    }
    Serial.println("\"}");
}

void serialInput() {
    while (Serial.available()) {
        char key = Serial.read();
        if (key == 'p')
            captureFrame();
        else if (key == 'n')
            ui.handle(indur::Event::Next);
        else if (key == 'b')
            ui.handle(indur::Event::Previous);
        else if (key == 's')
            ui.handle(indur::Event::Select);
        else if (key == 'x')
            ui.handle(indur::Event::Back);
        else if (key == '0')
            ui.home();
        else if (key == 'h')
            Serial.println("[UI] n=next b=previous s=select x=back 0=home p=frame");
    }
}

void setup() {
    neutralOutputs();
    Serial.begin(115200);
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(10);
    Wire1.begin(ENC_R_SDA, ENC_R_SCL);
    Wire1.setClock(400000);
    Wire1.setTimeOut(10);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(BTN_CENTER, INPUT_PULLUP);
    buttons.begin();
    // periphBegin=false preserves the explicitly selected mini robot bus pins.
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, true, false)) {
        Serial.println("[UI] OLED initialization failed");
        return;
    }
    registerInterface();
    if (!ui.begin()) {
        Serial.printf("[UI] Setup failed: %s\n", ui.errorText());
        return;
    }
    storageReady = storage.begin() && ui.attachStorage(storage, 0, storage.capacity(), 1);
    if (storageReady) {
        auto result = ui.load();
        if (result == indur::LoadResult::Empty)
            storageReady = ui.save();
        else if (result != indur::LoadResult::Loaded)
            ui.notify("CHECK DEMO STORAGE");
    }
    applySettings(0, indur::Change::Loaded, nullptr);
    ready = true;
    ui.tick(millis());
    ui.splash("MINI UI LAB", indur::icons::robot(), 900);
    Serial.println("[UI] MiniRobotTest ready. Motors neutral; original EEPROM untouched.");
    Serial.println("[UI] Press h for navigation and screenshot commands.");
}

void loop() {
    if (!ready) {
        delay(1);
        return;
    }
    uint32_t now = millis();
    if ((uint32_t)(now - lastSample) >= (uint32_t)sampleMs) {
        lastSample = now;
        for (uint8_t i = 0; i < 3; ++i)
            ir[i] = synthetic ? int(2047 + 1800 * sinf(now / 900.0f + i * 1.4f))
                              : analogRead(sensorPins[i]);
        angleLeft = readAngle(Wire);
        angleRight = readAngle(Wire1);
        uptime = now / 1000;
        heapBytes = ESP.getFreeHeap();
        static uint32_t lastLog = 0;
        if (logging && (uint32_t)(now - lastLog) >= 500) {
            lastLog = now;
            Serial.printf("[UI] IR=%d,%d,%d ENC=%d,%d HEAP=%lu\n", ir[0], ir[1], ir[2], angleLeft,
                          angleRight, (unsigned long)heapBytes);
        }
    }
    buttons.update(now);
    serialInput();
    now = millis();
    if (jobRunning && ui.view() != indur::View::Progress)
        jobRunning = false;
    if (jobRunning) {
        uint32_t elapsed = now - jobStart;
        if (elapsed >= 3000) {
            jobRunning = false;
            ui.home();
            ui.notify("UI DEMO COMPLETE");
        } else
            ui.progress("UI DEMO / NO MOTION", elapsed / 3000.0);
    }
    ui.tick(now);
    delay(1); // Yield to the ESP32 background tasks; no menu wait loop.
}
