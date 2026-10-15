#include <fstream>
#include <string>
#include <IndurUI.h>
#include <Adafruit_GFX.h>
#include <IndurUIGFX.h>
#include <IndurUIIcons.h>

// Uses unmodified Adafruit_GFX drawing/text, writing SSD1306 page-layout bytes.
class FrameDisplay : public Adafruit_GFX {
  public:
    uint8_t buffer[1024] = {};
    FrameDisplay() : Adafruit_GFX(128, 64) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || y < 0 || x >= 128 || y >= 64)
            return;
        uint8_t &value = buffer[x + (y / 8) * 128];
        uint8_t mask = uint8_t(1 << (y & 7));
        if (color)
            value |= mask;
        else
            value &= ~mask;
    }
    void display() {}
    void save(const std::string &path) const {
        static const char digits[] = "0123456789abcdef";
        std::ofstream out(path.c_str());
        out << "{\"format\":\"indurui-ssd1306-v1\",\"width\":128,\"height\":64,\"data\":\"";
        for (uint8_t byte : buffer)
            out << digits[byte >> 4] << digits[byte & 15];
        out << "\"}\n";
    }
};
void unused(void *) {}
int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    std::string directory = argv[1];
    FrameDisplay display;
    indur::GfxCanvas<FrameDisplay> canvas(display);
    indur::UI ui(canvas, "MINI UI LAB");
    int sensors[] = {1820, 2140, 2460, 2780, 3100}, left = 1350, right = 2720, speed = 60;
    int sampleMs = 50, uptime = 123;
    bool logging = false;
    float gain = 1.2f;
    ui.setInputStyle(indur::InputStyle::Original);
    ui.setHomeGroup(0);
    ui.addStatus("L", left);
    ui.addStatus("SP", speed);
    ui.addStatus("R", right);
    ui.addContext("Kp", gain, "", 1);
    ui.addContext("Ms", sampleMs);
    ui.addContext("Up", uptime);
    const char *labels[] = {"L", "LD", "F", "RD", "R"};
    for (int i = 0; i < 5; ++i)
        ui.addBar(labels[i], sensors[i], 0, 4095);
    ui.setWidgetGroup(ui.addValue("Left", left, "raw", 0), 1);
    ui.setWidgetGroup(ui.addValue("Right", right, "raw", 0), 1);
    ui.setWidgetGroup(ui.addValue("Uptime", uptime, "s", 0), 1);
    auto editable = ui.addNumber("speed", "Demo speed", speed, 0, 100, 5, "%");
    ui.addNumber("gain", "Demo gain", gain, 0, 10, .1, "", 1);
    ui.addToggle("logging", "Logging", logging);
    ui.addAction("Live sensors", unused, nullptr, indur::Root, indur::icons::sensors());
    ui.addSettings("Settings", indur::AllGroups, indur::Root, indur::icons::settings());
    ui.addAction("UI gallery", unused, nullptr, indur::Root, indur::icons::robot());
    ui.setThreeButtons(true);
    indur::Theme theme;
    theme.minWidgetWidth = 40;
    ui.setTheme(theme);
    if (!ui.begin())
        return 2;
    const auto capture = [&](const char *name) {
        ui.render();
        display.save(directory + "/" + name + ".json");
    };
    capture("dashboard");
    ui.openMenu();
    capture("menu");
    ui.openSettings();
    capture("settings");
    ui.edit(editable);
    ui.handle(indur::Event::Next);
    capture("editor");
    ui.handle(indur::Event::Back);
    ui.openDashboard(0, indur::DashboardLayout::Arc);
    capture("sensors");
    ui.openDashboard(1, indur::DashboardLayout::Cards);
    capture("telemetry");
    ui.openMenu();
    ui.setButtonHeld(1, true);
    capture("hold");
    ui.setButtonHeld(1, false);
    ui.home();
    ui.setAlert("LOW BATTERY");
    capture("alert");
    ui.setAlert(nullptr);
    ui.confirm("Start UI demo?", unused);
    capture("confirmation");
    ui.handle(indur::Event::Back);
    ui.progress("UI DEMO / NO MOTION", .62);
    capture("progress");
    ui.home();
    ui.notify("DEMO SETTINGS SAVED");
    capture("notification");
    ui.notify(nullptr, 0);
    ui.splash("MINI UI LAB", indur::icons::robot(), 1000);
    ui.tick(420);
    capture("splash");
    return 0;
}
