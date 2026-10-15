#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include "robotIcon.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "Custom Icons");
indur::Buttons buttons(ui, 2, 3, 4);
bool lights = false;
void toggleLights(void *) {
    lights = !lights;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        return;
    ui.addValue("Lights", lights);
    indur::Id item = ui.addAction("Robot lights", toggleLights);
    if (!ui.setIcon(item, robotIcon)) {
        Serial.println(ui.errorText());
        return;
    }
    indur::Theme theme;
    theme.iconSize = 32; // Slot size, independent of bitmap's original dimensions.
    ui.setTheme(theme);
    buttons.begin();
    if (!ui.begin())
        Serial.println(ui.errorText());
}
void loop() {
    uint32_t now = millis();
    buttons.update(now);
    ui.tick(now);
}
