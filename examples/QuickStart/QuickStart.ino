#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include <IndurUIIcons.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "My Robot");
// Active-low buttons connect pins 2, 3 and 4 to GND. Change for your board.
indur::Buttons buttons(ui, 2, 3, 4);

float battery = 7.8f;
int distance = 0;
int speed = 80;
bool enabled = false;

void toggleRobot(void *) {
    enabled = !enabled;
    ui.notify(enabled ? "ROBOT ENABLED" : "ROBOT DISABLED");
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED allocation/initialization failed");
        return;
    }
    ui.addValue("Battery", battery, "V", 2);
    ui.addBar("Distance", distance, 0, 1023);
    ui.addValue("Speed", speed, "%", 0);
    ui.addValue("Enabled", enabled);
    ui.addNumber("speed", "Speed limit", speed, 0, 100, 5, "%");
    ui.addToggle("enabled", "Enabled", enabled);
    ui.addAction("Run / Stop", toggleRobot, nullptr, indur::Root, indur::icons::run());
    ui.addSettings("Settings", indur::AllGroups, indur::Root, indur::icons::settings());
    buttons.begin();
    if (!ui.begin()) {
        Serial.println(ui.errorText());
        return;
    }
    ui.splash("My Robot", indur::icons::robot(), 800);
}

void loop() {
    uint32_t now = millis();
    distance = analogRead(A0); // Replace with your project's sensor updates.
    buttons.update(now);
    ui.tick(now);
    // Run your robot's other cooperative tasks here. No menu wait loop is needed.
}
