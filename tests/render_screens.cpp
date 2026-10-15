// Generate documentation from the library's real drawing commands.
#include <IndurUI.h>
#include <IndurUIIcons.h>
#include <fstream>
#include <sstream>
#include <string>
using namespace indur;
std::string escape(const char *s) {
    std::string result;
    for (; s && *s; ++s) {
        if (*s == '&')
            result += "&amp;";
        else if (*s == '<')
            result += "&lt;";
        else if (*s == '>')
            result += "&gt;";
        else if (*s == '\"')
            result += "&quot;";
        else
            result += *s;
    }
    return result;
}
struct SvgCanvas : Canvas {
    std::ostringstream s;
    const char *path;
    int16_t width() const override { return 128; }
    int16_t height() const override { return 64; }
    void clear() override {
        s.str("");
        s.clear();
    }
    void pixel(int16_t x, int16_t y, bool c) override { fillRect(x, y, 1, 1, c); }
    void shape(int x, int y, int w, int h, int r, bool filled, bool c) {
        if (w <= 0 || h <= 0)
            return;
        s << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' rx='"
          << r << "' fill='" << (filled ? (c ? "#e6fcff" : "#071117") : "none") << "' stroke='"
          << (c ? "#e6fcff" : "#071117") << "' stroke-width='.65'/>";
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool c) override {
        shape(x, y, w, h, 0, true, c);
    }
    void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool c) override {
        shape(x, y, w, h, 0, false, c);
    }
    void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, bool f, bool c) override {
        shape(x, y, w, h, r, f, c);
    }
    void text(int16_t x, int16_t y, const char *v, uint8_t size, bool c) override {
        if (!v || !*v)
            return;
        s << "<text x='" << x << "' y='" << y + 7 * size << "' font-size='" << 8 * size
          << "' font-family='monospace' textLength='" << strlen(v) * 6 * size
          << "' lengthAdjust='spacingAndGlyphs' fill='" << (c ? "#e6fcff" : "#071117") << "'>"
          << escape(v) << "</text>";
    }
    void present() override {
        std::ofstream out(path);
        out << "<svg xmlns='http://www.w3.org/2000/svg' width='768' height='448' viewBox='0 0 192 "
               "112'><rect width='192' height='112' rx='10' fill='#112630'/><rect x='16' y='16' "
               "width='160' height='80' rx='6' fill='#02080b'/><g transform='translate(32 24)'>"
            << s.str()
            << "</g><circle cx='8' cy='8' r='2' fill='#63edb5'/><circle cx='184' cy='104' r='2' "
               "fill='#365965'/></svg>";
    }
};
void nothing(void *) {}
int main() {
    SvgCanvas c;
    UI ui(c, "EXPLORER");
    float battery = 7.82;
    int rpm = 180;
    int sensors[] = {410, 730, 925, 620, 290};
    const char *names[] = {"LEFT", "FL", "FRONT", "FR", "RIGHT"};
    ui.addValue("BATTERY", battery, "V", 2);
    ui.addValue("MOTOR", rpm, "rpm", 0);
    for (int i = 0; i < 5; ++i)
        ui.addBar(names[i], sensors[i], 0, 1023);
    Id speed = ui.addNumber("rpm", "Speed limit", rpm, 0, 500, 10, "rpm");
    ui.addAction("Run robot", nothing, nullptr, Root, icons::robot());
    ui.addSettings("Settings", AllGroups, Root, icons::settings());
    ui.begin();
    c.path = "docs/assets/dashboard.svg";
    ui.render();
    c.path = "docs/assets/menu.svg";
    ui.openMenu();
    ui.render();
    c.path = "docs/assets/settings.svg";
    ui.openSettings();
    ui.render();
    c.path = "docs/assets/editor.svg";
    ui.edit(speed);
    ui.render();
    Theme theme;
    theme.minWidgetWidth = 24;
    ui.setTheme(theme);
    ui.setWidgetVisible(0, false);
    ui.setWidgetVisible(1, false);
    ui.home();
    c.path = "docs/assets/sensors.svg";
    ui.render();
    c.path = "docs/assets/confirmation.svg";
    ui.confirm("Calibrate now?", nothing);
    ui.render();
}
