#include "IndurUI.h"
#include "IndurUIButtons.h"
#include "IndurUIEEPROM.h"
#include <assert.h>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
using namespace indur;
int testPins[256], testModes[256];

struct TestCanvas : Canvas {
    int16_t w, h;
    int frames = 0;
    std::vector<std::string> labels;
    TestCanvas(int16_t width = 128, int16_t height = 64) : w(width), h(height) {}
    int16_t width() const override { return w; }
    int16_t height() const override { return h; }
    void bounds(int x, int y, int width, int height) {
        if (!width || !height)
            return;
        assert(x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= w && y + height <= h);
    }
    void clear() override {}
    void pixel(int16_t x, int16_t y, bool) override { bounds(x, y, 1, 1); }
    void fillRect(int16_t x, int16_t y, int16_t a, int16_t b, bool) override { bounds(x, y, a, b); }
    void rect(int16_t x, int16_t y, int16_t a, int16_t b, bool) override { bounds(x, y, a, b); }
    void roundRect(int16_t x, int16_t y, int16_t a, int16_t b, uint8_t, bool, bool) override {
        bounds(x, y, a, b);
    }
    void text(int16_t x, int16_t y, const char *s, uint8_t scale, bool) override {
        bounds(x, y, (int)strlen(s) * 6 * scale, 8 * scale);
        labels.push_back(s);
    }
    void present() override { ++frames; }
};
struct MemoryStorage : Storage {
    std::vector<uint8_t> bytes;
    int writes = 0, flushes = 0, reads = 0;
    int failWrite = -1, failRead = -1, failFlush = -1;
    explicit MemoryStorage(size_t size = 512) : bytes(size, 255) {}
    size_t capacity() const override { return bytes.size(); }
    bool read(size_t a, uint8_t &v) override {
        if (reads++ == failRead || a >= bytes.size())
            return false;
        v = bytes[a];
        return true;
    }
    bool write(size_t a, uint8_t v) override {
        if (writes++ == failWrite || a >= bytes.size())
            return false;
        bytes[a] = v;
        return true;
    }
    bool flush() override { return flushes++ != failFlush; }
};
void increment(void *p) {
    ++*static_cast<int *>(p);
}
void changed(Id, Change, void *p) {
    increment(p);
}
void drawCustom(Canvas &c, void *) {
    c.text(1, 1, "CUSTOM");
}
bool customEvent(Event e, void *p) {
    if (e == Event::Next) {
        increment(p);
        return true;
    }
    return false;
}
bool intercept(uint8_t button, Press press, void *p) {
    assert(button == 2 && press == Press::Click);
    increment(p);
    return true;
}

void testLayout() {
    const int sizes[][2] = {{64, 32},  {128, 32},  {128, 48}, {128, 60}, {128, 64},
                            {64, 128}, {128, 128}, {256, 64}, {256, 128}};
    for (const auto &size : sizes) {
        TestCanvas c(size[0], size[1]);
        UI ui(c, "Rover");
        int values[43] = {};
        for (int i = 0; i < 43; ++i)
            assert((i % 2 ? ui.addBar("SENSOR", values[i], 0, 1023)
                          : ui.addValue("SENSOR", values[i])) != Invalid);
        float gain = 1.2f;
        bool enabled = true;
        assert(ui.addNumber("gain", "Gain", gain, 0, 5, .1, "", 1) != Invalid);
        assert(ui.addToggle("enable", "Enabled", enabled) != Invalid);
        int calls = 0;
        Id sub = ui.addMenu("Tools");
        ui.addAction("Go", increment, &calls, sub);
        ui.addScreen("Custom", drawCustom, customEvent, &calls);
        assert(ui.begin());
        auto pages = ui.dashboardPages();
        assert(pages > 1);
        for (unsigned n = 0; n < pages; ++n) {
            ui.render();
            ui.handle(Event::Next);
        }
        assert(ui.dashboardPage() == 0);
        ui.openMenu();
        for (int n = 0; n < 7; ++n) {
            ui.render();
            ui.handle(Event::Next);
        }
        ui.setMenuStyle(MenuStyle::List);
        ui.render();
        ui.openSettings();
        ui.render();
        assert(ui.edit(0));
        ui.render();
        ui.handle(Event::Back);
        ui.confirm("Proceed?", increment, &calls);
        ui.render();
        ui.handle(Event::Next);
        ui.render();
        ui.handle(Event::Select);
        assert(calls == 1);
        ui.progress("Working", .5);
        ui.render();
        ui.handle(Event::Back);
        ui.splash("Robot", Icon(), 100);
        ui.render();
        ui.tick(101);
        assert(ui.view() == View::Dashboard);
        ui.notify("Saved", 100);
        ui.render();
        ui.tick(202);
        for (Id n = 0; n < 43; ++n)
            assert(ui.setWidgetVisible(n, false));
        assert(ui.dashboardPages() == 1);
        ui.render();
        ui.openSettings(17);
        ui.render();
        ui.openMenu(sub);
        ui.render();
        Theme t;
        t.minWidgetWidth = 24;
        t.minWidgetHeight = 22;
        t.iconSize = 255;
        ui.setTheme(t);
        ui.home();
        for (Id n = 0; n < 43; ++n)
            ui.setWidgetVisible(n, true);
        for (unsigned n = 0; n < ui.dashboardPages(); ++n) {
            ui.render();
            ui.handle(Event::Next);
        }
        ui.openMenu();
        ui.setMenuStyle(MenuStyle::Icons);
        ui.render();
        ui.splash("Wide", Icon(), 1);
        ui.render();
        TestCanvas empty(size[0], size[1]);
        UI blank(empty);
        assert(blank.begin());
        blank.openMenu();
        blank.render();
    }
}
void testBindingsAndEditor() {
    TestCanvas c;
    UI ui(c);
    int count = 5, calls = 0;
    bool flag = false;
    float gain = .5;
    const int constant = 4;
    volatile uint32_t ticks = 7;
    ui.addValue("Const", constant);
    ui.addValue("Ticks", ticks);
    Id a = ui.addNumber("count", "Count", count, 0, 10, 2);
    Id b = ui.addToggle("flag", "Flag", flag, 1);
    ui.addNumber("gain", "Gain", gain, 0, 1, .1, "", 1);
    const char *modes[] = {"Slow", "Fast"};
    uint8_t mode = 0;
    Id choice = ui.addChoice("mode", "Mode", mode, modes, 2);
    ui.onChange(changed, &calls);
    assert(ui.begin());
    assert(ui.menuCount() == 1);
    assert(ui.edit(a));
    ui.handle(Event::Next);
    assert(count == 5 && ui.pendingValue() == 7);
    ui.handle(Event::Back);
    assert(count == 5 && calls == 0);
    ui.edit(a);
    for (int i = 0; i < 10; ++i)
        ui.handle(Event::Next);
    assert(ui.pendingValue() == 10);
    ui.handle(Event::Select);
    assert(count == 10 && calls == 1);
    ui.edit(b);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    assert(flag);
    ui.edit(choice);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    assert(mode == 1);
    ui.setReadOnly(a, true);
    assert(!ui.edit(a));
    ui.openSettings(1);
    assert(ui.selectedSetting() == b);
    ui.resetDefaults(false);
    assert(count == 5 && !flag && mode == 0 && gain == .5);
    uint64_t big = UINT64_MAX;
    char text[32];
    Binding::ref(big).format(&big, text, sizeof(text), 0);
    assert(std::string(text) == "18446744073709551615");
    int64_t low = INT64_MIN;
    Binding::ref(low).format(&low, text, sizeof(text), 0);
    assert(std::string(text) == "-9223372036854775808");
    assert(ui.addNumber("count", "Duplicate", count, 0, 10) == Invalid &&
           ui.error() == Error::DuplicateKey);
    ui.clearError();
    uint8_t tiny = 0;
    assert(ui.addNumber("bad", "Bad", tiny, 0, 256) == Invalid);
    ui.clearError();
    assert(ui.addNumber("fraction", "Fraction", count, 0, 10, .5) == Invalid);
    uint8_t bitmap[] = {0x80, 0x80, 0x40, 0x00};
    Icon icon(bitmap, 9, 2, Memory::Ram);
    assert(icon.pixel(0, 0) && icon.pixel(8, 0) && icon.pixel(1, 1) && !icon.pixel(8, 1) &&
           !icon.pixel(9, 0));
}
void testNarrowNumbers() {
    TestCanvas narrow(64, 32);
    UI ui(narrow);
    Theme theme;
    theme.minWidgetWidth = 24;
    ui.setTheme(theme);
    ui.setDashboardLayout(DashboardLayout::Cards);
    int value = 123456;
    ui.addValue("Value", value);
    ui.addBar("Bar", value, 0, 999999);
    assert(ui.begin());
    ui.render();
    bool placeholder = false;
    for (const auto &label : narrow.labels) {
        assert(label != "1234" && label != "123");
        if (label == "--")
            placeholder = true;
    }
    assert(placeholder);
    TestCanvas text;
    UI strings(text);
    strings.addText("Status", "OK\nGO");
    strings.begin();
    strings.render();
    bool cleaned = false;
    for (const auto &label : text.labels)
        if (label == "OK?GO")
            cleaned = true;
    assert(cleaned);
}
void testMenus() {
    TestCanvas c;
    UI ui(c);
    int calls = 0;
    Id sub = ui.addMenu("Sub");
    Id action = ui.addAction("Run", increment, &calls, sub);
    ui.addScreen("Custom", drawCustom, customEvent, &calls);
    assert(ui.begin());
    ui.handle(Event::Select);
    ui.handle(Event::Select);
    ui.setEnabled(action, false);
    ui.handle(Event::Select);
    assert(calls == 0);
    ui.setEnabled(action, true);
    ui.handle(Event::Select);
    assert(calls == 1);
    ui.handle(Event::Back);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    assert(ui.view() == View::Custom);
    ui.render();
    ui.handle(Event::Next);
    assert(calls == 2);
    ui.handle(Event::Back);
    assert(ui.view() == View::Menu);
    ui.home();
    ui.shortcut(Event::Back, increment, &calls);
    ui.handle(Event::Back);
    assert(calls == 3);
    ui.confirm("Really?", increment, &calls);
    ui.handle(Event::Select);
    assert(calls == 3);
}
void testStorage() {
    TestCanvas c;
    MemoryStorage store;
    int value = 3;
    bool enabled = false;
    UI ui(c);
    ui.addNumber("n", "Number", value, 0, 99);
    ui.addToggle("b", "Enable", enabled);
    assert(ui.begin());
    assert(ui.requiredStorageBytes() == 2 * (20 + 6 + sizeof(int) + 6 + sizeof(bool)));
    assert(ui.attachStorage(store, 32, 400));
    assert(ui.load() == LoadResult::Empty);
    assert(ui.save());
    int writes = store.writes, flushes = store.flushes;
    assert(ui.save());
    assert(writes == store.writes && flushes == store.flushes);
    value = 8;
    enabled = true;
    assert(ui.save());
    value = 0;
    enabled = false;
    assert(ui.load() == LoadResult::Loaded && value == 8 && enabled);
    for (int i = 0; i < 32; ++i)
        assert(store.bytes[i] == 255);
    for (int i = 432; i < 512; ++i)
        assert(store.bytes[i] == 255);
    // Second generation is in bank 1. Damage its payload and recover bank 0.
    store.bytes[32 + 200 + 20] ^= 1;
    assert(ui.load() == LoadResult::Loaded && value == 3 && !enabled);
    assert(ui.edit(0));
    ui.handle(Event::Next);
    store.failWrite = store.writes;
    ui.handle(Event::Select);
    assert(value == 3 && ui.editing() && ui.error() == Error::StorageIO);
    store.failWrite = -1;
    ui.clearError();
    ui.handle(Event::Select);
    assert(value == 4);
    // Stable keys survive registration reordering and new settings retain defaults.
    UI reordered(c);
    int newValue = 77, n = 0;
    bool b = true;
    reordered.addToggle("b", "Enable", b);
    reordered.addNumber("new", "New", newValue, 0, 99);
    reordered.addNumber("n", "Number", n, 0, 99);
    assert(reordered.begin());
    assert(reordered.attachStorage(store, 32, 400));
    assert(reordered.load() == LoadResult::Loaded && n == 4 && !b && newValue == 77);
    MemoryStorage tiny(10);
    assert(!reordered.attachStorage(tiny) && reordered.error() == Error::StorageTooSmall);
    // Every interrupted write and either flush must preserve a valid older generation.
    MemoryStorage baseline;
    int old = 12;
    UI seed(c);
    seed.addNumber("v", "Value", old, 0, 99);
    assert(seed.begin());
    assert(seed.attachStorage(baseline));
    assert(seed.save());
    for (int fault = 0; fault < 55; ++fault) {
        MemoryStorage broken = baseline;
        broken.writes = 0;
        broken.flushes = 0;
        broken.reads = 0;
        broken.failWrite = fault;
        int v = 42;
        UI writer(c);
        writer.addNumber("v", "Value", v, 0, 99);
        assert(writer.begin());
        writer.attachStorage(broken);
        bool success = writer.save();
        broken.failWrite = -1;
        v = 0;
        assert(writer.load() == LoadResult::Loaded);
        assert(v == (success ? 42 : 12));
    }
    for (int fault = 0; fault < 2; ++fault) {
        MemoryStorage broken = baseline;
        broken.flushes = 0;
        broken.failFlush = fault;
        int v = 42;
        UI writer(c);
        writer.addNumber("v", "Value", v, 0, 99);
        writer.begin();
        writer.attachStorage(broken);
        assert(!writer.save());
        broken.failFlush = -1;
        v = 0;
        assert(writer.load() == LoadResult::Loaded && v == 12);
    }
    // Read failures cannot partially mutate live project values.
    for (int fault = 0; fault < 180; ++fault) {
        MemoryStorage broken = baseline;
        broken.reads = 0;
        broken.failRead = fault;
        int v = 55;
        UI reader(c);
        reader.addNumber("v", "Value", v, 0, 99);
        reader.begin();
        reader.attachStorage(broken);
        LoadResult result = reader.load();
        assert(result != LoadResult::IOError || v == 55);
    }
}
void testButtons() {
    ButtonFilter f;
    assert(f.update(true, 0) == Press::None);
    assert(f.update(false, 10) == Press::None);
    f.update(true, 20);
    f.update(true, 45);
    f.update(false, 100);
    assert(f.update(false, 125) == Press::Click);
    f.update(true, 200);
    f.update(true, 225);
    f.update(false, 600);
    assert(f.update(false, 625) == Press::Hold);
    f.update(true, 700);
    f.update(true, 725);
    assert(f.update(true, 1100, true) == Press::Repeat);
    f.update(false, 1200, true);
    assert(f.update(false, 1225, true) == Press::None);
    f.update(true, 1300);
    f.update(true, 1325);
    f.update(true, 6400);
    assert(f.stuck());
    f.update(false, 6500);
    assert(f.update(false, 6525) == Press::None && !f.stuck());
    ButtonFilter wrap;
    wrap.update(true, UINT32_MAX - 35);
    wrap.update(true, UINT32_MAX - 10);
    wrap.update(false, 20);
    assert(wrap.update(false, 45) == Press::Click);
    for (int center = -1; center <= 4; center += 5) {
        for (int &pin : testPins)
            pin = 1;
        TestCanvas c;
        UI ui(c);
        int v = 0;
        ui.addNumber("v", "Value", v, 0, 10);
        ui.begin();
        Buttons keys(ui, 2, 3, center);
        keys.begin();
        assert(testModes[2] == INPUT_PULLUP);
        // Holding right opens menu; releasing again cannot select an item twice.
        testPins[3] = 0;
        keys.update(10);
        keys.update(40);
        testPins[3] = 1;
        keys.update(450);
        keys.update(480);
        assert(ui.view() == View::Menu);
        ui.edit(0);
        testPins[3] = 0;
        keys.update(500);
        keys.update(530);
        keys.update(950);
        keys.update(1100);
        testPins[3] = 1;
        keys.update(1150);
        keys.update(1180);
        if (center < 0)
            assert(ui.view() == View::Settings && v == 0);
        else {
            assert(ui.editing() && ui.pendingValue() >= 2 && v == 0);
            int count = 0;
            keys.onPress(intercept, &count);
            testPins[4] = 0;
            keys.update(1250);
            keys.update(1280);
            testPins[4] = 1;
            keys.update(1350);
            keys.update(1380);
            assert(count == 1 && ui.editing());
            keys.onPress(nullptr);
        }
    }
}
struct RecordedCanvas : TestCanvas {
    RecordedCanvas(int16_t width = 128, int16_t height = 64) : TestCanvas(width, height) {}
    struct Text {
        int x, y;
        std::string value;
    };
    struct Box {
        int x, y, width, height;
        bool filled;
    };
    std::vector<Text> textCalls;
    std::vector<Box> boxes;
    void clear() override {
        labels.clear();
        textCalls.clear();
        boxes.clear();
    }
    void text(int16_t x, int16_t y, const char *s, uint8_t size, bool color) override {
        TestCanvas::text(x, y, s, size, color);
        textCalls.push_back({x, y, s});
    }
    void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, bool filled,
                   bool color) override {
        TestCanvas::roundRect(x, y, w, h, r, filled, color);
        boxes.push_back({x, y, w, h, filled});
    }
    bool has(const char *text) const {
        for (const auto &label : labels)
            if (label == text)
                return true;
        return false;
    }
};
void gridFormat(double value, char *out, size_t capacity, void *) {
    snprintf(out, capacity, "%dx%d", int(value), int(value));
}
void testOriginalPresentationAndHistory() {
    RecordedCanvas c;
    UI ui(c);
    ui.setInputStyle(InputStyle::Original);
    int fields[] = {111, 222, 333, 444, 555, 666}, values[] = {1, 2, 3, 4, 5};
    const char *labels[] = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon"};
    for (int i = 0; i < 3; ++i)
        ui.addStatus(labels[i], fields[i]);
    for (int i = 3; i < 6; ++i)
        ui.addContext(labels[i - 3], fields[i], "", 0, 4);
    for (int i = 0; i < 5; ++i)
        ui.addNumber(labels[i], labels[i], values[i], 0, 100, 1, "", 0, 4);
    int other = 777;
    ui.addContext("X", other, "", 0, 6);
    ui.addNumber("other", "Other", other, 0, 999, 1, "", 0, 6);
    ui.setSettingFormatter(0, gridFormat);
    int calls = 0;
    ui.addAction("First", increment, &calls);
    Id tools = ui.addMenu("Tools");
    ui.addAction("Action", increment, &calls, tools);
    Id settings = ui.addSettings("Tuning", 4, tools);
    Id custom = ui.addScreen("Inspect", drawCustom, nullptr, nullptr, tools);
    Id sensors = ui.addDashboard("Readings", 0, DashboardLayout::Arc, tools);
    assert(ui.begin());
    ui.openMenu();
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    ui.render();
    assert(c.has("Alpha") && c.has("Beta") && c.has("Gamma") && c.has("1x1") && !c.has("Delta"));
    bool thirdRow = false;
    for (const auto &text : c.textCalls)
        if (text.value == "Gamma" && text.y == 36)
            thirdRow = true;
    assert(thirdRow);
    for (int i = 0; i < 4; ++i)
        ui.handle(Event::Next);
    ui.render();
    assert(c.has("Gamma") && c.has("Delta") && c.has("Epsilon") && !c.has("Alpha"));
    ui.handle(Event::Select);
    ui.render();
    bool editorBox = false;
    for (const auto &box : c.boxes)
        if (box.x == 0 && box.y == 26 && box.width == 128 && box.height == 27)
            editorBox = true;
    assert(editorBox && c.has("Epsilon") && c.has("SAVE") && !c.has("X:777"));
    ui.handleButton(0, Press::Click);
    assert(ui.pendingValue() == 6 && values[4] == 5);
    ui.handleButton(1, Press::Click);
    assert(ui.pendingValue() == 5);
    ui.handleButton(0, Press::Hold);
    assert(ui.selectedSetting() == 4 && values[4] == 5);
    ui.handle(Event::Back);
    assert(ui.selectedMenuItem() == settings);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    ui.handle(Event::Back);
    assert(ui.selectedMenuItem() == custom);
    ui.handle(Event::Next);
    ui.handle(Event::Select);
    ui.handle(Event::Back);
    assert(ui.selectedMenuItem() == sensors);
    ui.handle(Event::Back);
    assert(ui.selectedMenuItem() == tools);
    ui.edit(5);
    ui.render();
    assert(c.has("X:777"));
}
void testContentLayouts() {
    const int sizes[][2] = {{64, 32},  {128, 32}, {128, 48},  {128, 60},
                            {128, 64}, {64, 128}, {128, 128}, {256, 64}};
    for (const auto &size : sizes)
        for (auto style : {InterfaceStyle::Original, InterfaceStyle::Cards}) {
            TestCanvas c(size[0], size[1]);
            UI ui(c);
            Theme theme;
            theme.style = style;
            theme.statusRows = 2;
            theme.minWidgetWidth = 24;
            ui.setTheme(theme);
            int data[43] = {};
            for (int i = 0; i < 26; ++i) {
                data[i] = 1000 + i;
                Id id = i < 17 ? ui.addBar("IR", data[i], 0, 4095) : ui.addValue("Value", data[i]);
                ui.setWidgetGroup(id, i % 2);
            }
            for (int i = 26; i < 36; ++i)
                ui.addStatus("S", data[i]);
            for (int i = 36; i < 43; ++i)
                ui.addContext("C", data[i]);
            ui.addNumber("n", "Tuning", data[0], 0, 4095);
            assert(ui.begin());
            for (auto layout : {DashboardLayout::Auto, DashboardLayout::Cards,
                                DashboardLayout::Bars, DashboardLayout::Arc}) {
                ui.openDashboard(1, layout);
                unsigned pages = ui.dashboardPages();
                assert(pages > 1);
                for (unsigned page = 0; page < pages; ++page) {
                    ui.render();
                    ui.handle(Event::Next);
                }
                assert(ui.dashboardPage() == 0);
                ui.handle(Event::Back);
                assert(ui.view() == View::Dashboard);
            }
            ui.edit(0);
            ui.render();
            ui.tick(2600);
            ui.render();
            ui.setButtonHeld(1, true);
            ui.render();
            ui.setButtonHeld(1, false);
            ui.home();
            ui.setAlert("LOW BATTERY");
            ui.render();
            ui.setAlert(nullptr);
            ui.render();
        }
    RecordedCanvas c;
    UI ui(c);
    int sensors[7] = {1, 2, 3, 4, 5, 6, 7}, value = 99;
    for (int &sensor : sensors)
        ui.addBar("IR", sensor, 0, 4095);
    Id diagnostics = ui.addValue("Private group", value);
    ui.setWidgetGroup(diagnostics, 1);
    ui.setHomeGroup(0);
    assert(ui.begin());
    assert(ui.dashboardPages() == 2);
    ui.render();
    assert(!c.has("Private group"));
    ui.openDashboard(1, DashboardLayout::Cards);
    ui.render();
    assert(c.has("Private group"));
    assert(!ui.setWidgetGroup(diagnostics, AllGroups));
    assert(!ui.setWidgetPlacement(Invalid, WidgetPlacement::Status));
}
void testOriginalPhysicalButtons() {
    for (int center : {-1, 4}) {
        for (int &pin : testPins)
            pin = 1;
        RecordedCanvas c;
        UI ui(c);
        int value = 5;
        ui.addNumber("value", "Value", value, 0, 10);
        ui.setInputStyle(InputStyle::Original);
        Buttons buttons(ui, 2, 3, center);
        buttons.begin();
        assert(ui.begin());
        uint32_t now = 0;
        auto press = [&](int pin, bool hold) {
            testPins[pin] = 0;
            buttons.update(now += 50);
            buttons.update(now += 30);
            buttons.update(now += hold ? 400 : 40);
            if (hold) {
                ui.render();
                assert(c.has("Release to confirm"));
            }
            testPins[pin] = 1;
            buttons.update(now += 10);
            buttons.update(now += 30);
        };
        ui.edit(0);
        press(2, false);
        assert(ui.pendingValue() == 6 && value == 5);
        press(3, true);
        assert(value == 6 && ui.view() == View::Settings);
        press(3, true);
        assert(ui.editing());
        press(2, false);
        press(2, true);
        assert(value == 6 && ui.view() == View::Settings);
        int calls[6] = {};
        ui.home();
        for (uint8_t i = 0; i < 6; ++i)
            assert(ui.bindHomeButton(i / 2, i % 2 ? Press::Hold : Press::Click, "ACT", increment,
                                     &calls[i]));
        for (uint8_t i = 0; i < 6; ++i) {
            ui.handleButton(i / 2, i % 2 ? Press::Hold : Press::Click);
            assert(calls[i] == 1);
        }
        ui.openMenu();
        ui.handleButton(1, Press::Click);
        assert(calls[2] == 1);
        assert(!ui.bindHomeButton(3, Press::Click, "X", increment));
        assert(!ui.bindHomeButton(0, Press::Repeat, "X", increment));
    }
}
void testUnitFormattingKeepsAllDigits() {
    RecordedCanvas canvas(64, 64);
    UI ui(canvas);
    int value = 123456;
    ui.addNumber("wide", "Long value", value, 0, 999999, 1, "unit that cannot fit");
    assert(ui.begin());
    ui.openSettings();
    ui.render();
    assert(canvas.has("123456"));
    ui.edit(0);
    ui.render();
    assert(canvas.has("123456"));
}
struct BasicEeprom {
    uint8_t data[32] = {};
    uint8_t read(size_t a) { return data[a]; }
    void write(size_t a, uint8_t v) { data[a] = v; }
    size_t length() { return 32; }
};
struct BoolEeprom : BasicEeprom {
    bool commit() { return false; }
};
struct VoidEeprom : BasicEeprom {
    void commit() {}
};
void testAdapters() {
    BasicEeprom e;
    EEPROMStorage<BasicEeprom> store(e, 100);
    uint8_t v;
    assert(store.capacity() == 32 && store.write(0, 7) && store.read(0, v) && v == 7 &&
           store.flush());
    assert(!store.write(32, 1));
    BoolEeprom b;
    EEPROMStorage<BoolEeprom> failing(b, 32);
    assert(!failing.flush());
    VoidEeprom n;
    EEPROMStorage<VoidEeprom> passing(n, 32);
    assert(passing.flush());
}
int main() {
    testLayout();
    testBindingsAndEditor();
    testNarrowNumbers();
    testMenus();
    testStorage();
    testButtons();
    testAdapters();
    testOriginalPresentationAndHistory();
    testContentLayouts();
    testOriginalPhysicalButtons();
    testUnitFormattingKeepsAllDigits();
    std::cout << "IndurUI native checks passed\n";
}
