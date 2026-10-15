#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "Two Buttons");
indur::Buttons buttons(ui, 2, 3); // Omit center pin: full navigation with two buttons.
float target = 25.0f;
bool logging = false;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        return;
    ui.addValue("Target", target, "C", 1);
    ui.addValue("Logging", logging);
    ui.addNumber("target", "Target", target, -10, 80, .5, "C", 1);
    ui.addToggle("logging", "Logging", logging);
    buttons.begin();
    if (!ui.begin())
        Serial.println(ui.errorText());
}

void loop() {
    uint32_t now = millis();
    buttons.update(now);
    ui.tick(now);
    // Tap left/right to change a value. Hold right then release to save/select.
    // Hold left then release to cancel/back. Edits are pending until saved.
}
