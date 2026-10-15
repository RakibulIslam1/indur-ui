#pragma once

#include "IndurUIBoard.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define INDURUI_PROGMEM PROGMEM
#elif defined(ESP8266)
#include <pgmspace.h>
#define INDURUI_PROGMEM PROGMEM
#else
#define INDURUI_PROGMEM
#endif

namespace indur {

typedef uint16_t Id;
static const Id Invalid = 0xffff;
static const Id Root = 0xfffe;
static const uint8_t AllGroups = 255;

enum class Event : uint8_t { Previous, Next, Select, Back };
enum class Press : uint8_t { None, Click, Hold, Repeat };
enum class InterfaceStyle : uint8_t { Original, Cards };
enum class InputStyle : uint8_t { Standard, Original };
enum class DashboardLayout : uint8_t { Auto, Cards, Bars, Arc };
enum class WidgetPlacement : uint8_t { Body, Status, Editor, StatusAndEditor };
enum class View : uint8_t {
    Dashboard,
    Menu,
    Settings,
    Editor,
    Custom,
    Confirmation,
    Progress,
    Splash
};
enum class MenuStyle : uint8_t { Icons, List };
enum class WidgetStyle : uint8_t { Value, Bar, Boolean, Text };
enum class Memory : uint8_t { Ram, Flash };
enum class Error : uint8_t {
    None,
    OutOfMemory,
    InvalidArgument,
    DuplicateKey,
    StorageTooSmall,
    StorageIO,
    CorruptData
};
enum class LoadResult : uint8_t { Loaded, Empty, Corrupt, IOError, NotAttached, OutOfMemory };
enum class Change : uint8_t { Edited, Loaded, Defaults };

struct Icon {
    const uint8_t *data;
    uint16_t width;
    uint16_t height;
    Memory memory;
    Icon(const uint8_t *p = 0, uint16_t w = 0, uint16_t h = 0, Memory m = Memory::Flash)
        : data(p), width(w), height(h), memory(m) {}
    bool valid() const { return data && width && height && width <= 256 && height <= 256; }
    bool pixel(uint16_t x, uint16_t y) const;
};

class Canvas {
  public:
    virtual ~Canvas() {}
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual void clear() = 0;
    virtual void pixel(int16_t x, int16_t y, bool color = true) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool color = true) = 0;
    virtual void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color = true) = 0;
    virtual void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r,
                           bool filled = false, bool color = true) = 0;
    virtual void text(int16_t x, int16_t y, const char *value, uint8_t size = 1,
                      bool color = true) = 0;
    virtual void present() = 0;
};

class Storage {
  public:
    virtual ~Storage() {}
    virtual size_t capacity() const = 0;
    virtual bool read(size_t address, uint8_t &value) = 0;
    virtual bool write(size_t address, uint8_t value) = 0;
    virtual bool flush() = 0;
};

typedef void (*Action)(void *context);
typedef double (*NumberReader)(void *context);
typedef void (*TextReader)(void *context, char *output, size_t capacity);
typedef void (*ScreenDraw)(Canvas &canvas, void *context);
typedef bool (*ScreenEvent)(Event event, void *context);
typedef void (*ChangeHandler)(Id setting, Change reason, void *context);
typedef void (*ValueFormatter)(double value, char *output, size_t capacity, void *context);

namespace detail {
void unsignedText(uint64_t value, char *out, size_t capacity, bool negative = false);
void decimalText(double value, uint8_t decimals, char *out, size_t capacity);
template <typename T> uint8_t kind(const volatile T *) {
    return T(-1) < T(0) ? 1 : 2;
}
inline uint8_t kind(const volatile bool *) {
    return 4;
}
inline uint8_t kind(const volatile float *) {
    return 3;
}
inline uint8_t kind(const volatile double *) {
    return 3;
}
inline uint8_t kind(const volatile long double *) {
    return 3;
}
bool fits(double value, uint8_t kind, uint8_t bytes);
} // namespace detail

struct Binding {
    void *context;
    double (*read)(void *);
    bool (*write)(void *, double);
    void (*format)(void *, char *, size_t, uint8_t);
    void (*bytes)(void *, uint8_t *, bool);
    double (*decode)(const uint8_t *);
    uint8_t type;
    uint8_t size;

    Binding() : context(0), read(0), write(0), format(0), bytes(0), decode(0), type(0), size(0) {}

    template <typename T> static Binding ref(T &value) {
        static_assert(sizeof(T) <= 8, "Use a text/callback widget for larger objects");
        Binding b;
        b.context = (void *)&value;
        b.read = &get<T>;
        b.write = &put<T>;
        b.format = &print<T>;
        b.bytes = &transfer<T>;
        b.decode = &unpack<T>;
        b.type = detail::kind(&value);
        b.size = sizeof(T);
        return b;
    }
    template <typename T> static Binding ref(const T &value) {
        Binding b = ref(const_cast<T &>(value));
        b.write = 0;
        b.bytes = 0;
        return b;
    }

    static Binding computed(NumberReader reader, void *context);

  private:
    template <typename T> static double get(void *p) { return (double)*static_cast<T *>(p); }
    template <typename T> static bool put(void *p, double n) {
        if (!detail::fits(n, detail::kind(static_cast<T *>(p)), sizeof(T)))
            return false;
        *static_cast<T *>(p) = (T)n;
        return true;
    }
    template <typename T> static void print(void *p, char *out, size_t capacity, uint8_t decimals) {
        T value = *static_cast<T *>(p);
        uint8_t type = detail::kind(&value);
        if (type == 4) {
            const char *label = value ? "ON" : "OFF";
            if (capacity) {
                strncpy(out, label, capacity - 1);
                out[capacity - 1] = 0;
            }
        } else if (type == 3) {
            detail::decimalText((double)value, decimals, out, capacity);
        } else if (type == 1 && value < T(0)) {
            int64_t n = (int64_t)value;
            detail::unsignedText((uint64_t)(-(n + 1)) + 1, out, capacity, true);
        } else {
            detail::unsignedText((uint64_t)value, out, capacity);
        }
    }
    template <typename T> static void transfer(void *p, uint8_t *data, bool restore) {
        volatile uint8_t *raw = (volatile uint8_t *)static_cast<T *>(p);
        for (uint8_t i = 0; i < sizeof(T); ++i) {
            if (restore)
                raw[i] = data[i];
            else
                data[i] = raw[i];
        }
    }
    template <typename T> static double unpack(const uint8_t *data) {
        T value = T();
        volatile uint8_t *raw = (volatile uint8_t *)&value;
        for (uint8_t i = 0; i < sizeof(T); ++i)
            raw[i] = data[i];
        return (double)value;
    }
};

struct Theme {
    InterfaceStyle style;
    uint8_t radius;
    uint8_t minWidgetWidth;
    uint8_t minWidgetHeight;
    uint8_t iconSize;
    uint16_t refreshMs;
    uint8_t minBarWidth, minStatusWidth, statusRows;
    uint16_t contextPageMs;
    Theme()
        : style(InterfaceStyle::Original), radius(3), minWidgetWidth(48), minWidgetHeight(22),
          iconSize(32), refreshMs(100), minBarWidth(24), minStatusWidth(40), statusRows(1),
          contextPageMs(2500) {}
};

class UI {
  public:
    explicit UI(Canvas &canvas, const char *title = "Robot");
    ~UI();
    UI(const UI &) = delete;
    UI &operator=(const UI &) = delete;

    template <typename T>
    Id addValue(const char *label, T &value, const char *unit = "", uint8_t decimals = 1) {
        return addWidget(label, Binding::ref(value), unit, decimals, WidgetStyle::Value, 0, 1, 0,
                         0);
    }
    Id addValue(const char *label, NumberReader read, void *context, const char *unit = "",
                uint8_t decimals = 1);
    template <typename T>
    Id addBar(const char *label, T &value, double minimum, double maximum, const char *unit = "") {
        return addWidget(label, Binding::ref(value), unit, 0, WidgetStyle::Bar, minimum, maximum, 0,
                         0);
    }
    Id addBar(const char *label, NumberReader read, void *context, double minimum, double maximum,
              const char *unit = "");
    Id addText(const char *label, TextReader read, void *context = 0);
    Id addText(const char *label, const char *liveText);
    template <typename T>
    Id addStatus(const char *label, T &value, const char *unit = "", uint8_t decimals = 0) {
        Id id = addValue(label, value, unit, decimals);
        if (id != Invalid)
            setWidgetPlacement(id, WidgetPlacement::StatusAndEditor);
        return id;
    }
    template <typename T>
    Id addContext(const char *label, T &value, const char *unit = "", uint8_t decimals = 0,
                  uint8_t group = AllGroups) {
        Id id = addValue(label, value, unit, decimals);
        if (id != Invalid) {
            setWidgetPlacement(id, WidgetPlacement::Editor);
            setWidgetContextGroup(id, group);
        }
        return id;
    }
    template <typename T>
    Id addNumber(const char *key, const char *label, T &value, double minimum, double maximum,
                 double step = 1, const char *unit = "", uint8_t decimals = 0, uint8_t group = 0) {
        return addSetting(key, label, Binding::ref(value), minimum, maximum, step, unit, decimals,
                          group, 0, 0);
    }
    Id addToggle(const char *key, const char *label, bool &value, uint8_t group = 0);
    template <typename T>
    Id addChoice(const char *key, const char *label, T &value, const char *const *labels,
                 uint8_t count, uint8_t group = 0) {
        if (!labels || !count)
            return addSetting(key, label, Binding::ref(value), 1, 0, 1, "", 0, group, 0, 0);
        return addSetting(key, label, Binding::ref(value), 0, count ? count - 1 : 0, 1, "", 0,
                          group, labels, count);
    }

    Id addAction(const char *label, Action action, void *context = 0, Id parent = Root,
                 Icon icon = Icon());
    Id addMenu(const char *label, Id parent = Root, Icon icon = Icon());
    Id addSettings(const char *label = "Settings", uint8_t group = AllGroups, Id parent = Root,
                   Icon icon = Icon());
    Id addScreen(const char *label, ScreenDraw draw, ScreenEvent event = 0, void *context = 0,
                 Id parent = Root, Icon icon = Icon());
    Id addDashboard(const char *label, uint8_t group,
                    DashboardLayout layout = DashboardLayout::Auto, Id parent = Root,
                    Icon icon = Icon());
    bool setIcon(Id menuItem, Icon icon);
    bool setEnabled(Id menuItem, bool enabled);
    bool setWidgetVisible(Id widget, bool visible);
    bool setWidgetPlacement(Id widget, WidgetPlacement placement);
    bool setWidgetGroup(Id widget, uint8_t group);
    bool setWidgetContextGroup(Id widget, uint8_t settingsGroup);
    bool setReadOnly(Id setting, bool readOnly);
    bool setSettingFormatter(Id setting, ValueFormatter formatter, void *context = 0);
    void onChange(ChangeHandler handler, void *context = 0);
    void shortcut(Event event, Action action, void *context = 0);
    void setTitle(const char *title);
    void setTheme(const Theme &theme);
    void setMenuStyle(MenuStyle style);
    void setThreeButtons(bool enabled);
    void setInputStyle(InputStyle style);
    InputStyle inputStyle() const;
    bool bindHomeButton(uint8_t button, Press press, const char *label, Action action,
                        void *context = 0);
    void handleButton(uint8_t button, Press press);
    void setButtonHeld(uint8_t button, bool held);
    void setHomeGroup(uint8_t group);
    void setDashboardLayout(DashboardLayout layout);
    void setAlert(const char *message);
    void setAutoSave(bool enabled);

    bool begin();
    void tick(uint32_t now);
    void render();
    void handle(Event event);
    void home();
    void openMenu(Id parent = Root);
    void openDashboard(uint8_t group, DashboardLayout layout = DashboardLayout::Auto);
    void openSettings(uint8_t group = AllGroups);
    bool edit(Id setting);
    void notify(const char *message, uint32_t duration = 1200);
    void splash(const char *title, Icon icon = Icon(), uint32_t duration = 1000);
    void progress(const char *title, double fraction);
    void confirm(const char *question, Action yes, void *context = 0);
    bool attachStorage(Storage &storage, size_t offset = 0, size_t bytes = 0, uint32_t schema = 1);
    size_t requiredStorageBytes() const;
    LoadResult load();
    bool save();
    bool resetDefaults(bool persist = true);

    View view() const;
    bool editing() const { return view() == View::Editor; }
    uint16_t widgetCount() const;
    uint16_t settingCount() const;
    uint16_t menuCount() const;
    uint16_t dashboardPages() const;
    uint16_t dashboardPage() const;
    Id selectedSetting() const;
    Id selectedMenuItem() const;
    Error error() const;
    const char *errorText() const;
    void clearError();
    double pendingValue() const;

  private:
    struct Impl;
    Impl *p_;
    Id addWidget(const char *, Binding, const char *, uint8_t, WidgetStyle, double, double,
                 TextReader, void *);
    Id addSetting(const char *, const char *, Binding, double, double, double, const char *,
                  uint8_t, uint8_t, const char *const *, uint8_t);
    Id addEntry(const char *, uint8_t, Action, ScreenDraw, ScreenEvent, void *, Id, Icon, uint8_t);
};

struct ButtonTiming {
    uint16_t debounceMs, holdMs, repeatMs, stuckMs;
    ButtonTiming() : debounceMs(25), holdMs(350), repeatMs(120), stuckMs(5000) {}
};

class ButtonFilter {
  public:
    explicit ButtonFilter(ButtonTiming timing = ButtonTiming());
    Press update(bool pressed, uint32_t now, bool repeat = false);
    bool stuck() const { return stuck_; }
    bool held(uint32_t now) const {
        return stable_ && !stuck_ && (uint32_t)(now - down_) >= timing_.holdMs;
    }

  private:
    ButtonTiming timing_;
    bool raw_, stable_, repeated_, stuck_;
    uint32_t changed_, down_, lastRepeat_;
};

} // namespace indur
