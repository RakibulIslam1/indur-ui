#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include <IndurUIIcons.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "Explorer");
indur::Buttons buttons(ui, 2, 3, 4);
const char *sensorNames[] = {"L", "LD", "F", "RD", "R"};
int sensors[5] = {};
long encoder = 0;
float temperature = 23.6f;
int speed = 70;
bool diagnostics = true;
uint8_t mode = 0;
const char *modeLabels[] = {"Explore", "Follow", "Manual"};
bool calibrating = false;
uint32_t calibrationStart = 0;

void status(void *, char *out, size_t capacity) {
    snprintf(out, capacity, "%s", calibrating ? "CALIBRATING" : "READY");
}
double temperatureF(void *) {
    return temperature * 1.8 + 32;
}
void startCalibration(void *) {
    calibrating = true;
    calibrationStart = millis();
    ui.progress("Calibrating", 0);
}
void askCalibration(void *) {
    ui.confirm("Calibrate now?", startCalibration);
}
void drawDiagnostics(indur::Canvas &c, void *) {
    c.text(2, 2, "Diagnostics");
    char row[22];
    snprintf(row, sizeof(row), "Encoder: %ld", encoder);
    c.text(2, 16, row);
    snprintf(row, sizeof(row), "Mode: %s", modeLabels[mode]);
    c.text(2, 29, row);
    c.text(2, 52, "Hold left: back");
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        return;
    // The original instrument layout keeps status, sensor bars, and diagnostics separate.
    indur::Theme theme;
    theme.minWidgetWidth = 64;
    theme.minBarWidth = 24;
    ui.setTheme(theme);
    ui.setInputStyle(indur::InputStyle::Original);
    ui.setHomeGroup(0);
    ui.addStatus("Enc", encoder);
    ui.addStatus("SP", speed);
    ui.addStatus("T", temperature, "", 1);
    ui.addContext("Mode", mode);
    ui.addContext("Dbg", diagnostics);
    ui.addContext("SP", speed);
    for (uint8_t i = 0; i < 5; ++i)
        ui.addBar(sensorNames[i], sensors[i], 0, 1023);
    ui.setWidgetGroup(ui.addValue("Encoder", encoder, "ticks", 0), 1);
    ui.setWidgetGroup(ui.addValue("Temp", temperatureF, nullptr, "F", 1), 1);
    ui.setWidgetGroup(ui.addText("Status", status), 1);
    ui.addNumber("speed", "Speed", speed, 0, 100, 5, "%", 0, 1);
    ui.addToggle("debug", "Diagnostics", diagnostics, 2);
    ui.addChoice("mode", "Mode", mode, modeLabels, 3, 1);
    indur::Id tools = ui.addMenu("Tools", indur::Root, indur::icons::tools());
    ui.addDashboard("Sensor readings", 0, indur::DashboardLayout::Arc, tools,
                    indur::icons::sensors());
    ui.addDashboard("Telemetry", 1, indur::DashboardLayout::Cards, tools, indur::icons::info());
    ui.addAction("Calibrate", askCalibration, nullptr, tools, indur::icons::sensors());
    ui.addSettings("Motion", 1, tools, indur::icons::settings());
    ui.addSettings("Diagnostics", 2, tools, indur::icons::info());
    ui.addScreen("Live diagnostics", drawDiagnostics, nullptr, nullptr, indur::Root,
                 indur::icons::robot());
    buttons.begin();
    if (!ui.begin())
        Serial.println(ui.errorText());
}

void loop() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 5; ++i)
        sensors[i] = (now / 10 + i * 175) % 1024;
    encoder = now / 10; // Demo values: replace with a synchronized encoder snapshot.
    buttons.update(now);
    now = millis();
    if (calibrating) {
        uint32_t elapsed = now - calibrationStart;
        if (elapsed >= 2500) {
            calibrating = false;
            ui.home();
            ui.notify("CALIBRATION DONE");
        } else if (ui.view() == indur::View::Progress)
            ui.progress("Calibrating", elapsed / 2500.0);
        // Leaving the progress view does not cancel application-owned work.
    }
    ui.tick(now);
}
