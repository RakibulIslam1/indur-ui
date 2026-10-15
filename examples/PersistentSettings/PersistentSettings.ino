#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_SSD1306.h>
#include <IndurUI.h>
#include <IndurUIGFX.h>
#include <IndurUIButtons.h>
#include <IndurUIEEPROM.h>
#include <IndurUIIcons.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
indur::GfxCanvas<Adafruit_SSD1306> canvas(display);
indur::UI ui(canvas, "Saved Settings");
indur::Buttons buttons(ui, 2, 3, 4);
float gain = 1.2f;
uint16_t limit = 180;
bool sound = true;

void changed(indur::Id, indur::Change, void *) {
    // Apply derived configuration here; do not register more items in callbacks.
    Serial.println("Configuration updated");
}
void restore(void *) {
    if (ui.resetDefaults())
        ui.notify("DEFAULTS SAVED");
    else
        ui.notify("SAVE FAILED");
}
void askReset(void *) {
    ui.confirm("Reset settings?", restore);
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        return;
#if defined(ESP32) || defined(ESP8266)
    if (!EEPROM.begin(1024)) {
        Serial.println("EEPROM begin failed");
        return;
    }
#endif
    // Construct only after EEPROM.begin() on emulated EEPROM boards.
    static indur::EEPROMStorage<decltype(EEPROM)> storage(EEPROM, EEPROM.length());
    ui.addValue("Gain", gain, "", 1);
    ui.addValue("Limit", limit, "rpm", 0);
    ui.addNumber("gain", "Gain", gain, 0, 10, .1, "", 1);
    ui.addNumber("limit", "Speed limit", limit, 0, 500, 10, "rpm");
    ui.addToggle("sound", "Sound", sound);
    ui.addSettings("Settings", indur::AllGroups, indur::Root, indur::icons::settings());
    ui.addAction("Restore defaults", askReset, nullptr, indur::Root, indur::icons::storage());
    ui.onChange(changed);
    buttons.begin();
    if (!ui.begin()) {
        Serial.println(ui.errorText());
        return;
    }
    // Bytes 0..63 and 576 onward stay available to your application.
    if (!ui.attachStorage(storage, 64, 512, 1)) {
        Serial.println(ui.errorText());
        return;
    }
    indur::LoadResult result = ui.load();
    if (result == indur::LoadResult::Empty) {
        if (!ui.save())
            Serial.println(ui.errorText());
    } else if (result != indur::LoadResult::Loaded) {
        Serial.println(ui.errorText());
        ui.notify("CHECK SAVED DATA"); // Preserve data; offer deliberate recovery via menu.
    }
}

void loop() {
    uint32_t now = millis();
    buttons.update(now);
    ui.tick(now);
}
