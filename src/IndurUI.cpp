#include "IndurUI.h"
#include <math.h>
#include <new>
#include <stdio.h>

namespace indur {
namespace {
template <typename T> T minimum(T a, T b) {
    return a < b ? a : b;
}
template <typename T> T maximum(T a, T b) {
    return a > b ? a : b;
}
double clamp(double v, double lo, double hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
bool finiteNumber(double v) {
    return !isnan(v) && !isinf(v);
}
void copyText(char *out, size_t n, const char *s) {
    if (n) {
        strncpy(out, s ? s : "", n - 1);
        out[n - 1] = 0;
    }
}
uint32_t hashKey(const char *s) {
    uint32_t h = 2166136261UL;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619UL;
    }
    return h;
}
uint32_t crcByte(uint32_t crc, uint8_t value) {
    crc ^= value;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    return crc;
}
void little32(uint8_t *p, uint32_t n) {
    for (uint8_t i = 0; i < 4; ++i)
        p[i] = (uint8_t)(n >> (8 * i));
}
uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
template <typename T> struct Array {
    T *data;
    uint16_t count;
    Array() : data(0), count(0) {}
    ~Array() { delete[] data; }
    bool push(const T &item) {
        if (count >= 65000 || (uint32_t)(count + 1) * sizeof(T) > (size_t)-1)
            return false;
        T *next = new (std::nothrow) T[count + 1];
        if (!next)
            return false;
        for (uint16_t i = 0; i < count; ++i)
            next[i] = data[i];
        next[count] = item;
        delete[] data;
        data = next;
        ++count;
        return true;
    }
    T &operator[](uint16_t i) { return data[i]; }
    const T &operator[](uint16_t i) const { return data[i]; }
};
struct Widget {
    const char *label, *unit;
    Binding binding;
    TextReader text;
    void *context;
    double lo, hi;
    uint8_t decimals;
    WidgetStyle style;
    bool visible;
};
struct Setting {
    const char *label, *unit;
    Binding binding;
    const char *const *choices;
    double lo, hi, step;
    uint32_t key;
    uint8_t defaults[8];
    uint8_t decimals, group, choiceCount;
    bool readOnly;
};
struct Entry {
    const char *label;
    Action action;
    ScreenDraw draw;
    ScreenEvent event;
    void *context;
    Icon icon;
    Id parent;
    uint8_t kind, group;
    bool enabled;
};
struct Shortcut {
    Action action;
    void *context;
};
struct Buffer {
    uint8_t *data;
    explicit Buffer(size_t bytes) : data(new (std::nothrow) uint8_t[bytes]) {}
    ~Buffer() { delete[] data; }
};
static const uint8_t JournalMagic[4] = {'I', 'U', 'I', 1};
static const uint8_t HeaderBytes = 20;
struct Bank {
    uint8_t state;
    uint32_t generation;
    uint16_t payload, records;
    size_t base;
};
} // namespace

namespace detail {
void unsignedText(uint64_t value, char *out, size_t capacity, bool negative) {
    if (!capacity)
        return;
    char reversed[21];
    uint8_t count = 0;
    do {
        reversed[count++] = char('0' + value % 10);
        value /= 10;
    } while (value && count < 20);
    size_t pos = 0;
    if (negative && pos + 1 < capacity)
        out[pos++] = '-';
    while (count && pos + 1 < capacity)
        out[pos++] = reversed[--count];
    out[pos] = 0;
}
void decimalText(double value, uint8_t decimals, char *out, size_t capacity) {
    if (!finiteNumber(value)) {
        copyText(out, capacity, isnan(value) ? "NaN" : value < 0 ? "-Inf" : "Inf");
        return;
    }
    if (!capacity)
        return;
    decimals = minimum<uint8_t>(decimals, 6);
    bool negative = value < 0;
    double n = fabs(value);
    if (n >= 1e9 || (n > 0 && n < 1e-6)) {
        int exponent = 0;
        while (n >= 10) {
            n /= 10;
            ++exponent;
        }
        while (n < 1 && n > 0) {
            n *= 10;
            --exponent;
        }
        char mantissa[20], suffix[12];
        decimalText(negative ? -n : n, minimum<uint8_t>(decimals, 3), mantissa, sizeof(mantissa));
        snprintf(suffix, sizeof(suffix), "e%+d", exponent);
        copyText(out, capacity, mantissa);
        size_t used = strlen(out);
        copyText(out + used, capacity - used, suffix);
        return;
    }
    uint32_t scale = 1;
    for (uint8_t i = 0; i < decimals; ++i)
        scale *= 10;
    uint64_t rounded = (uint64_t)(n * scale + 0.5);
    char whole[24];
    unsignedText(rounded / scale, whole, sizeof(whole), negative && rounded != 0);
    copyText(out, capacity, whole);
    size_t pos = strlen(out);
    if (decimals && pos + 1 < capacity) {
        out[pos++] = '.';
        uint32_t fraction = (uint32_t)(rounded % scale);
        for (uint8_t i = 0; i < decimals && pos + 1 < capacity; ++i) {
            scale /= 10;
            out[pos++] = char('0' + fraction / scale);
            fraction %= scale;
        }
        out[pos] = 0;
    }
}
bool fits(double n, uint8_t type, uint8_t bytes) {
    if (!finiteNumber(n))
        return false;
    if (type == 4)
        return n == 0 || n == 1;
    if (type == 3)
        return bytes > 4 || fabs(n) <= 3.4028234e38;
    double upper = ldexp(1.0, bytes * 8 - (type == 1 ? 1 : 0));
    return n < upper && n >= (type == 1 ? -upper : 0);
}
} // namespace detail

Binding Binding::computed(NumberReader reader, void *context) {
    Binding b;
    b.read = reader;
    b.context = context;
    b.type = 3;
    return b;
}

bool Icon::pixel(uint16_t x, uint16_t y) const {
    if (!valid() || x >= width || y >= height)
        return false;
    size_t offset = (size_t)y * ((width + 7) / 8) + x / 8;
    uint8_t b;
#if defined(__AVR__) || defined(ESP8266)
    b = memory == Memory::Flash ? pgm_read_byte(data + offset) : data[offset];
#else
    b = data[offset];
#endif
    return (b & (0x80 >> (x & 7))) != 0;
}

struct UI::Impl {
    Canvas &c;
    Array<Widget> widgets;
    Array<Setting> settings;
    Array<Entry> entries;
    const char *title, *message, *modalTitle;
    Theme theme;
    View state, returnView;
    MenuStyle menuStyle;
    Id parent, selected, editingId, customId;
    uint16_t page;
    uint8_t group;
    double pending, fraction;
    bool three, autosave, dirty, confirmYes, begun;
    uint32_t now, lastFrame, messageAt, messageDuration, splashAt, splashDuration;
    Icon splashIcon;
    Error fault;
    Action confirmed;
    void *confirmedContext;
    ChangeHandler changed;
    void *changeContext;
    Shortcut shortcuts[4];
    Storage *storage;
    size_t storageOffset, storageBytes;
    uint32_t schema;

    Impl(Canvas &canvas, const char *caption)
        : c(canvas), title(caption), message(0), modalTitle(0), state(View::Dashboard),
          returnView(View::Dashboard), menuStyle(MenuStyle::Icons), parent(Root), selected(0),
          editingId(Invalid), customId(Invalid), page(0), group(AllGroups), pending(0), fraction(0),
          three(false), autosave(true), dirty(true), confirmYes(false), begun(false), now(0),
          lastFrame(0), messageAt(0), messageDuration(0), splashAt(0), splashDuration(0),
          fault(Error::None), confirmed(0), confirmedContext(0), changed(0), changeContext(0),
          storage(0), storageOffset(0), storageBytes(0), schema(1) {
        for (uint8_t i = 0; i < 4; ++i) {
            shortcuts[i].action = 0;
            shortcuts[i].context = 0;
        }
    }
    bool fail(Error e) {
        fault = e;
        return false;
    }
    bool validParent(Id id) const {
        return id == Root || (id < entries.count && entries[id].kind == 1);
    }
    uint16_t countWidgets() const {
        uint16_t n = 0;
        for (uint16_t i = 0; i < widgets.count; ++i)
            if (widgets[i].visible)
                ++n;
        return n;
    }
    uint16_t countEntries() const {
        uint16_t n = 0;
        for (uint16_t i = 0; i < entries.count; ++i)
            if (entries[i].parent == parent)
                ++n;
        return n;
    }
    uint16_t countSettings() const {
        uint16_t n = 0;
        for (uint16_t i = 0; i < settings.count; ++i)
            if (group == AllGroups || settings[i].group == group)
                ++n;
        return n;
    }
    Id entryAt(uint16_t position) const {
        for (Id i = 0; i < entries.count; ++i)
            if (entries[i].parent == parent && position-- == 0)
                return i;
        return Invalid;
    }
    Id settingAt(uint16_t position) const {
        for (Id i = 0; i < settings.count; ++i)
            if ((group == AllGroups || settings[i].group == group) && position-- == 0)
                return i;
        return Invalid;
    }
    uint16_t cellsPerPage() const {
        int16_t h = c.height() < 48 ? c.height() - 12 : c.height() - 24;
        uint16_t columns =
            maximum<uint16_t>(1, c.width() / maximum<uint8_t>(24, theme.minWidgetWidth));
        uint16_t rows = maximum<uint16_t>(1, h / maximum<uint8_t>(22, theme.minWidgetHeight));
        return columns * rows;
    }
    uint16_t pages() const {
        return maximum<uint16_t>(1,
                                 ((uint32_t)countWidgets() + cellsPerPage() - 1) / cellsPerPage());
    }
    void textFit(int16_t x, int16_t y, int16_t width, const char *s, uint8_t size = 1,
                 bool color = true) {
        char text[64];
        size_t chars = minimum<size_t>(sizeof(text) - 1, width > 0 ? width / (6 * size) : 0);
        if (!chars)
            return;
        copyText(text, chars + 1, s);
        for (size_t i = 0; text[i]; ++i)
            if ((uint8_t)text[i] < 32 || (uint8_t)text[i] > 126)
                text[i] = '?';
        if (s && strlen(s) > chars && chars >= 3) {
            text[chars - 1] = '.';
            text[chars - 2] = '.';
            text[chars - 3] = '.';
        }
        c.text(x, y, text, size, color);
    }
    void centered(int16_t y, const char *s, uint8_t size = 1) {
        int16_t w =
            (int16_t)(minimum<size_t>((c.width() - 4) / (6 * size), strlen(s ? s : "")) * 6 * size);
        textFit((c.width() - w) / 2, y, w, s, size);
    }
    void valueFit(int16_t x, int16_t y, int16_t width, const char *s, uint8_t size = 1,
                  bool color = true) {
        // A truncated number could be mistaken for a different measurement.
        textFit(x, y, width, strlen(s) * 6 * size <= (size_t)maximum<int16_t>(0, width) ? s : "--",
                size, color);
    }
    void footer(const char *left, const char *right, const char *middle = "") {
        int16_t y = c.height() - 10, side = (c.width() - 6) / 3;
        c.roundRect(0, y, side, 10, theme.radius, true, true);
        c.roundRect(c.width() - side, y, side, 10, theme.radius, true, true);
        c.roundRect(side + 3, y, c.width() - 2 * side - 6, 10, theme.radius, false, true);
        textFit(2, y + 1, side - 4, left, 1, false);
        textFit(c.width() - side + 2, y + 1, side - 4, right, 1, false);
        textFit(side + 5, y + 1, c.width() - 2 * side - 10, middle);
    }
    void header(const char *label) {
        c.roundRect(0, 0, c.width(), 11, theme.radius, false, true);
        textFit(3, 2, c.width() - 6, label);
    }
    void drawIcon(Icon icon, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (!icon.valid() || w < 1 || h < 1)
            return;
        double scale = minimum((double)w / icon.width, (double)h / icon.height);
        int16_t dw = maximum<int16_t>(1, (int16_t)(icon.width * scale));
        int16_t dh = maximum<int16_t>(1, (int16_t)(icon.height * scale));
        x += (w - dw) / 2;
        y += (h - dh) / 2;
        for (int16_t iy = 0; iy < dh; ++iy)
            for (int16_t ix = 0; ix < dw; ++ix)
                if (icon.pixel((uint16_t)((uint32_t)ix * icon.width / dw),
                               (uint16_t)((uint32_t)iy * icon.height / dh)))
                    c.pixel(x + ix, y + iy, true);
    }
    void format(const Setting &s, double number, char *out, size_t n, bool live) {
        if (s.choiceCount && finiteNumber(number)) {
            int index = (int)clamp(number, 0, s.choiceCount - 1);
            copyText(out, n, s.choices[index]);
        } else if (live && s.binding.format)
            s.binding.format(s.binding.context, out, n, s.decimals);
        else if (s.binding.type == 4)
            copyText(out, n, number ? "ON" : "OFF");
        else
            detail::decimalText(number, s.decimals, out, n);
    }
    void dashboard() {
        bool compact = c.height() < 48;
        if (!compact)
            header(title);
        uint16_t total = countWidgets();
        page %= pages();
        uint16_t first = page * cellsPerPage(),
                 visible = minimum<uint16_t>(cellsPerPage(), total > first ? total - first : 0);
        if (!total)
            centered(compact ? 4 : 24, "Add widgets");
        uint16_t cols = minimum<uint16_t>(
            maximum<uint16_t>(1, c.width() / maximum<uint8_t>(24, theme.minWidgetWidth)),
            maximum<uint16_t>(1, visible));
        uint16_t rows = maximum<uint16_t>(1, (visible + cols - 1) / cols);
        uint16_t ordinal = 0, painted = 0;
        for (uint16_t i = 0; i < widgets.count && painted < visible; ++i) {
            Widget &w = widgets[i];
            if (!w.visible || ordinal++ < first)
                continue;
            uint16_t column = painted % cols, row = painted / cols;
            int16_t x = column * c.width() / cols, endX = (column + 1) * c.width() / cols;
            int16_t top = compact ? 0 : 13, available = c.height() - top - 12;
            int16_t y = top + row * available / rows;
            int16_t bottom = top + (row + 1) * available / rows;
            int16_t width = endX - x - 2, height = bottom - y - (compact ? 1 : 2);
            c.roundRect(x, y, width, height, theme.radius, false, true);
            textFit(x + 3, y + (compact ? 1 : 2), width - 6, w.label);
            char value[48] = {};
            if (w.text)
                w.text(w.context, value, sizeof(value));
            else if (w.binding.format)
                w.binding.format(w.binding.context, value, sizeof(value), w.decimals);
            else
                detail::decimalText(w.binding.read(w.binding.context), w.decimals, value,
                                    sizeof(value));
            value[sizeof(value) - 1] = 0;
            if (w.style == WidgetStyle::Bar) {
                double n = w.binding.read(w.binding.context);
                double f = finiteNumber(n) ? clamp((n - w.lo) / (w.hi - w.lo), 0, 1) : 0;
                c.rect(x + 3, y + height - 7, width - 6, 4, true);
                c.fillRect(x + 4, y + height - 6, (int16_t)((width - 8) * f), 2, true);
                if (height >= 29)
                    valueFit(x + 3, y + 12, width - 6, value);
            } else {
                uint8_t size =
                    height >= 33 && strlen(value) * 12 <= (size_t)maximum<int16_t>(0, width - 6)
                        ? 2
                        : 1;
                if (w.style == WidgetStyle::Text)
                    textFit(x + 3, y + (compact ? 10 : 12), width - 6, value, size);
                else
                    valueFit(x + 3, y + (compact ? 10 : 12), width - 6, value, size);
                if (height >= 34 && w.unit && *w.unit && size == 1)
                    textFit(x + 3, y + height - 10, width - 6, w.unit);
            }
            ++painted;
        }
        char counter[18];
        snprintf(counter, sizeof(counter), "%u/%u", (unsigned)(page + 1), (unsigned)pages());
        footer("PREV", "NEXT", three && pages() == 1 ? "MENU" : counter);
    }
    void menu() {
        uint16_t count = countEntries();
        if (!count) {
            header("Menu");
            centered(c.height() < 48 ? 12 : 25, "No menu items");
            footer("BACK", "SELECT");
            return;
        }
        selected %= count;
        Entry &item = entries[entryAt(selected)];
        if (menuStyle == MenuStyle::Icons && c.height() >= 48) {
            int16_t size = minimum<int16_t>(
                c.width() - 10,
                minimum<int16_t>(theme.iconSize, maximum<int16_t>(8, c.height() - 30)));
            if (item.icon.valid())
                drawIcon(item.icon, (c.width() - size) / 2, 2, size, size);
            else {
                c.roundRect((c.width() - size) / 2, 2, size, size, theme.radius, false, true);
                char n[8];
                snprintf(n, sizeof(n), "%u", (unsigned)(selected + 1));
                centered(2 + (size - 8) / 2, n);
            }
            centered(c.height() - 22, item.label);
            int16_t h = maximum<int16_t>(6, c.height() - 28);
            c.fillRect(c.width() - 3, 2, 1, h, true);
            c.fillRect(c.width() - 4,
                       2 + (int32_t)selected * (h - 4) / maximum<uint16_t>(1, count - 1), 3, 4,
                       true);
        } else {
            header(parent == Root ? "Menu" : entries[parent].label);
            int16_t top = c.height() < 48 ? 11 : 13, rowHeight = c.height() < 48 ? 10 : 14;
            uint16_t rows = maximum<int16_t>(1, (c.height() - top - 11) / rowHeight),
                     first = selected / rows * rows;
            for (uint16_t i = 0; i < rows && first + i < count; ++i) {
                Entry &e = entries[entryAt(first + i)];
                bool active = first + i == selected;
                int16_t y = top + i * rowHeight;
                c.roundRect(1, y, c.width() - 5, rowHeight - 1, theme.radius, active, true);
                textFit(4, y + 1, c.width() - 12, e.label, 1, !active);
            }
        }
        if (three)
            footer("PREV", "NEXT", item.enabled ? "OK" : "LOCK");
        else
            footer("BACK", item.enabled ? "SELECT" : "LOCKED", "-/+");
    }
    void settingsView() {
        header("Settings");
        uint16_t count = countSettings();
        if (!count)
            centered(c.height() < 48 ? 12 : 24, "No settings");
        else {
            selected %= count;
            int16_t top = c.height() < 48 ? 11 : 13, rowHeight = c.height() < 48 ? 10 : 14;
            uint16_t rows = maximum<int16_t>(1, (c.height() - top - 11) / rowHeight),
                     first = selected / rows * rows;
            for (uint16_t i = 0; i < rows && first + i < count; ++i) {
                Setting &s = settings[settingAt(first + i)];
                bool active = first + i == selected;
                int16_t y = top + i * rowHeight,
                        labelWidth = maximum<int16_t>(24, (c.width() - 8) * 3 / 5);
                c.roundRect(1, y, c.width() - 3, rowHeight - 1, theme.radius, active, true);
                textFit(4, y + 1, labelWidth - 6, s.label, 1, !active);
                char value[40];
                format(s, s.binding.read(s.binding.context), value, sizeof(value), true);
                if (s.choiceCount)
                    textFit(labelWidth, y + 1, c.width() - labelWidth - 5, value, 1, !active);
                else
                    valueFit(labelWidth, y + 1, c.width() - labelWidth - 5, value, 1, !active);
            }
        }
        if (three)
            footer("PREV", "NEXT", "EDIT");
        else
            footer("BACK", "EDIT", "-/+");
    }
    void editor() {
        Setting &s = settings[editingId];
        header(s.label);
        char value[40];
        format(s, pending, value, sizeof(value), false);
        uint8_t size = c.height() >= 48 && strlen(value) * 12 <= (size_t)c.width() - 6 ? 2 : 1;
        centered(c.height() < 48 ? 12 : 16, value, size);
        if (c.height() >= 60)
            centered(36, s.unit);
        if (three)
            footer("-", "+", "SAVE");
        else
            footer("CANCEL", "SAVE", "-/+");
    }
    size_t payloadSize() const {
        size_t n = 0;
        for (uint16_t i = 0; i < settings.count; ++i)
            n += 6 + settings[i].binding.size;
        return n;
    }
    bool get(size_t address, uint8_t &v) { return storage && storage->read(address, v); }
    bool put(size_t address, uint8_t v) {
        uint8_t old;
        return get(address, old) && (old == v || storage->write(address, v));
    }
    Bank bank(uint8_t index) {
        Bank b = {};
        b.base = storageOffset + index * (storageBytes / 2);
        uint8_t header[HeaderBytes];
        for (uint8_t i = 0; i < HeaderBytes; ++i)
            if (!get(b.base + i, header[i])) {
                b.state = 3;
                return b;
            }
        if (memcmp(header, JournalMagic, 4)) {
            b.state = 0;
            return b;
        }
        b.generation = read32(header + 4);
        b.payload = header[8] | (uint16_t)header[9] << 8;
        b.records = header[10] | (uint16_t)header[11] << 8;
        if (read32(header + 12) != schema || (size_t)b.payload + HeaderBytes > storageBytes / 2) {
            b.state = 2;
            return b;
        }
        uint32_t crc = 0xffffffffUL;
        for (uint8_t i = 4; i < 16; ++i)
            crc = crcByte(crc, header[i]);
        for (size_t i = 0; i < b.payload; ++i) {
            uint8_t v;
            if (!get(b.base + HeaderBytes + i, v)) {
                b.state = 3;
                return b;
            }
            crc = crcByte(crc, v);
        }
        if ((crc ^ 0xffffffffUL) != read32(header + 16)) {
            b.state = 2;
            return b;
        }
        size_t pos = 0;
        for (uint16_t i = 0; i < b.records; ++i) {
            uint8_t type, size;
            if (pos + 6 > b.payload) {
                b.state = 2;
                return b;
            }
            if (!get(b.base + HeaderBytes + pos + 4, type) ||
                !get(b.base + HeaderBytes + pos + 5, size)) {
                b.state = 3;
                return b;
            }
            if (type < 1 || type > 4 || !size || size > 8 || pos + 6 + size > b.payload) {
                b.state = 2;
                return b;
            }
            pos += 6 + size;
        }
        b.state = pos == b.payload ? 1 : 2;
        return b;
    }
    Bank newest(Bank a, Bank b) {
        if (a.state != 1)
            return b;
        if (b.state != 1)
            return a;
        return (int32_t)(a.generation - b.generation) > 0 ? a : b;
    }
    bool save() {
        if (!storage)
            return fail(Error::StorageIO);
        if (payloadSize() + HeaderBytes > storageBytes / 2)
            return fail(Error::StorageTooSmall);
        for (Id i = 0; i < settings.count; ++i) {
            Setting &s = settings[i];
            double v = s.binding.read(s.binding.context);
            if (!finiteNumber(v) || v < s.lo || v > s.hi)
                return fail(Error::InvalidArgument);
        }
        Bank a = bank(0), b = bank(1);
        if (a.state == 3 || b.state == 3)
            return fail(Error::StorageIO);
        Bank current = newest(a, b);
        size_t size = payloadSize();
        Buffer snapshot(size ? size : 1);
        if (!snapshot.data)
            return fail(Error::OutOfMemory);
        size_t offset = 0;
        for (Id i = 0; i < settings.count; ++i) {
            Setting &s = settings[i];
            little32(snapshot.data + offset, s.key);
            snapshot.data[offset + 4] = s.binding.type;
            snapshot.data[offset + 5] = s.binding.size;
            s.binding.bytes(s.binding.context, snapshot.data + offset + 6, false);
            offset += 6 + s.binding.size;
        }
        bool same =
            current.state == 1 && current.payload == size && current.records == settings.count;
        for (size_t i = 0; same && i < size; ++i) {
            uint8_t v;
            if (!get(current.base + HeaderBytes + i, v))
                return fail(Error::StorageIO);
            same = v == snapshot.data[i];
        }
        if (same) {
            fault = Error::None;
            return true;
        }
        size_t target = current.state == 1 && current.base == storageOffset
                            ? storageOffset + storageBytes / 2
                            : storageOffset;
        uint8_t h[HeaderBytes] = {};
        memcpy(h, JournalMagic, 4);
        little32(h + 4, current.state == 1 ? current.generation + 1 : 1);
        h[8] = (uint8_t)size;
        h[9] = (uint8_t)(size >> 8);
        h[10] = (uint8_t)settings.count;
        h[11] = (uint8_t)(settings.count >> 8);
        little32(h + 12, schema);
        uint32_t crc = 0xffffffffUL;
        for (uint8_t i = 4; i < 16; ++i)
            crc = crcByte(crc, h[i]);
        for (size_t i = 0; i < size; ++i)
            crc = crcByte(crc, snapshot.data[i]);
        little32(h + 16, crc ^ 0xffffffffUL);
        for (uint8_t i = 0; i < 4; ++i)
            if (!put(target + i, 0))
                return fail(Error::StorageIO);
        for (uint8_t i = 4; i < HeaderBytes; ++i)
            if (!put(target + i, h[i]))
                return fail(Error::StorageIO);
        for (size_t i = 0; i < size; ++i)
            if (!put(target + HeaderBytes + i, snapshot.data[i]))
                return fail(Error::StorageIO);
        if (!storage->flush())
            return fail(Error::StorageIO);
        for (uint8_t i = 0; i < 4; ++i)
            if (!put(target + i, JournalMagic[i]))
                return fail(Error::StorageIO);
        if (!storage->flush()) {
            for (uint8_t i = 0; i < 4; ++i)
                put(target + i, 0);
            storage->flush();
            return fail(Error::StorageIO);
        }
        fault = Error::None;
        return true;
    }
};

UI::UI(Canvas &canvas, const char *title)
    : p_(new (std::nothrow) Impl(canvas, title ? title : "Robot")) {}
UI::~UI() {
    delete p_;
}

Id UI::addWidget(const char *label, Binding binding, const char *unit, uint8_t decimals,
                 WidgetStyle style, double lo, double hi, TextReader reader, void *context) {
    if (!p_)
        return Invalid;
    if (!label || (!binding.read && !reader) ||
        (style == WidgetStyle::Bar && (!finiteNumber(lo) || !finiteNumber(hi) || hi <= lo))) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    Widget w = {};
    w.label = label;
    w.unit = unit;
    w.binding = binding;
    w.decimals = minimum<uint8_t>(6, decimals);
    w.style = style;
    w.lo = lo;
    w.hi = hi;
    w.text = reader;
    w.context = context;
    w.visible = true;
    if (!p_->widgets.push(w)) {
        p_->fail(Error::OutOfMemory);
        return Invalid;
    }
    p_->dirty = true;
    return p_->widgets.count - 1;
}
Id UI::addValue(const char *s, NumberReader r, void *ctx, const char *u, uint8_t d) {
    return addWidget(s, Binding::computed(r, ctx), u, d, WidgetStyle::Value, 0, 1, 0, 0);
}
Id UI::addBar(const char *s, NumberReader r, void *ctx, double lo, double hi, const char *u) {
    return addWidget(s, Binding::computed(r, ctx), u, 0, WidgetStyle::Bar, lo, hi, 0, 0);
}
Id UI::addText(const char *s, TextReader r, void *ctx) {
    return addWidget(s, Binding(), "", 0, WidgetStyle::Text, 0, 1, r, ctx);
}
static void liveText(void *ctx, char *out, size_t n) {
    copyText(out, n, static_cast<const char *>(ctx));
}
Id UI::addText(const char *s, const char *text) {
    return addText(s, liveText, (void *)text);
}

Id UI::addSetting(const char *key, const char *label, Binding b, double lo, double hi, double step,
                  const char *unit, uint8_t decimals, uint8_t group, const char *const *choices,
                  uint8_t count) {
    if (!p_)
        return Invalid;
    if (!key || !*key || !label || !b.read || !b.write || !b.bytes || !b.decode || !b.size ||
        !finiteNumber(lo) || !finiteNumber(hi) || !finiteNumber(step) || lo > hi || step <= 0 ||
        group == AllGroups || (choices && !count)) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    if (b.type != 3 && (floor(step) != step || floor(lo) != lo || floor(hi) != hi)) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    if (!detail::fits(lo, b.type, b.size) || !detail::fits(hi, b.type, b.size)) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    double initial = b.read(b.context);
    if (!finiteNumber(initial) || initial < lo || initial > hi) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    uint32_t hash = hashKey(key);
    for (Id i = 0; i < p_->settings.count; ++i)
        if (p_->settings[i].key == hash) {
            p_->fail(Error::DuplicateKey);
            return Invalid;
        }
    Setting s = {};
    s.label = label;
    s.unit = unit;
    s.binding = b;
    s.lo = lo;
    s.hi = hi;
    s.step = step;
    s.key = hash;
    s.group = group;
    s.decimals = minimum<uint8_t>(6, decimals);
    s.choices = choices;
    s.choiceCount = count;
    b.bytes(b.context, s.defaults, false);
    if (!p_->settings.push(s)) {
        p_->fail(Error::OutOfMemory);
        return Invalid;
    }
    return p_->settings.count - 1;
}
Id UI::addToggle(const char *key, const char *label, bool &v, uint8_t group) {
    return addSetting(key, label, Binding::ref(v), 0, 1, 1, "", 0, group, 0, 0);
}
Id UI::addEntry(const char *label, uint8_t kind, Action action, ScreenDraw draw, ScreenEvent event,
                void *context, Id parent, Icon icon, uint8_t group) {
    if (!p_)
        return Invalid;
    if (!label || !p_->validParent(parent) || (kind == 0 && !action) || (kind == 3 && !draw)) {
        p_->fail(Error::InvalidArgument);
        return Invalid;
    }
    Entry e = {};
    e.label = label;
    e.kind = kind;
    e.action = action;
    e.draw = draw;
    e.event = event;
    e.context = context;
    e.parent = parent;
    e.icon = icon;
    e.group = group;
    e.enabled = true;
    if (!p_->entries.push(e)) {
        p_->fail(Error::OutOfMemory);
        return Invalid;
    }
    return p_->entries.count - 1;
}
Id UI::addAction(const char *s, Action a, void *ctx, Id parent, Icon icon) {
    return addEntry(s, 0, a, 0, 0, ctx, parent, icon, 0);
}
Id UI::addMenu(const char *s, Id parent, Icon icon) {
    return addEntry(s, 1, 0, 0, 0, 0, parent, icon, 0);
}
Id UI::addSettings(const char *s, uint8_t g, Id parent, Icon icon) {
    return addEntry(s, 2, 0, 0, 0, 0, parent, icon, g);
}
Id UI::addScreen(const char *s, ScreenDraw d, ScreenEvent e, void *ctx, Id parent, Icon icon) {
    return addEntry(s, 3, 0, d, e, ctx, parent, icon, 0);
}
bool UI::setIcon(Id id, Icon icon) {
    if (!p_ || id >= p_->entries.count || (icon.data && !icon.valid()))
        return p_ ? p_->fail(Error::InvalidArgument) : false;
    p_->entries[id].icon = icon;
    p_->dirty = true;
    return true;
}
bool UI::setEnabled(Id id, bool v) {
    if (!p_ || id >= p_->entries.count)
        return false;
    p_->entries[id].enabled = v;
    p_->dirty = true;
    return true;
}
bool UI::setWidgetVisible(Id id, bool v) {
    if (!p_ || id >= p_->widgets.count)
        return false;
    p_->widgets[id].visible = v;
    p_->dirty = true;
    return true;
}
bool UI::setReadOnly(Id id, bool v) {
    if (!p_ || id >= p_->settings.count)
        return false;
    p_->settings[id].readOnly = v;
    return true;
}
void UI::onChange(ChangeHandler h, void *ctx) {
    if (p_) {
        p_->changed = h;
        p_->changeContext = ctx;
    }
}
void UI::shortcut(Event e, Action a, void *ctx) {
    if (p_ && (uint8_t)e < 4) {
        p_->shortcuts[(uint8_t)e].action = a;
        p_->shortcuts[(uint8_t)e].context = ctx;
    }
}
void UI::setTitle(const char *s) {
    if (p_ && s) {
        p_->title = s;
        p_->dirty = true;
    }
}
void UI::setTheme(const Theme &t) {
    if (p_) {
        p_->theme = t;
        p_->theme.refreshMs = maximum<uint16_t>(1, t.refreshMs);
        p_->dirty = true;
    }
}
void UI::setMenuStyle(MenuStyle s) {
    if (p_) {
        p_->menuStyle = s;
        p_->dirty = true;
    }
}
void UI::setThreeButtons(bool v) {
    if (p_) {
        p_->three = v;
        p_->dirty = true;
    }
}
void UI::setAutoSave(bool v) {
    if (p_)
        p_->autosave = v;
}
bool UI::begin() {
    if (!p_ || p_->fault != Error::None)
        return false;
    if (p_->c.width() < 64 || p_->c.height() < 32 || p_->c.width() > 1024 || p_->c.height() > 1024)
        return p_->fail(Error::InvalidArgument);
    bool hasSettings = false;
    for (Id i = 0; i < p_->entries.count; ++i)
        if (p_->entries[i].kind == 2)
            hasSettings = true;
    if (p_->settings.count && !hasSettings && addSettings() == Invalid)
        return false;
    p_->begun = true;
    p_->dirty = true;
    return true;
}
void UI::tick(uint32_t now) {
    if (!p_ || !p_->begun)
        return;
    p_->now = now;
    if (p_->state == View::Splash && (uint32_t)(now - p_->splashAt) >= p_->splashDuration)
        home();
    if (p_->message && (uint32_t)(now - p_->messageAt) >= p_->messageDuration) {
        p_->message = 0;
        p_->dirty = true;
    }
    if (p_->dirty || (uint32_t)(now - p_->lastFrame) >= p_->theme.refreshMs)
        render();
}
void UI::render() {
    if (!p_ || !p_->begun)
        return;
    p_->c.clear();
    switch (p_->state) {
    case View::Dashboard:
        p_->dashboard();
        break;
    case View::Menu:
        p_->menu();
        break;
    case View::Settings:
        p_->settingsView();
        break;
    case View::Editor:
        p_->editor();
        break;
    case View::Custom:
        if (p_->customId < p_->entries.count)
            p_->entries[p_->customId].draw(p_->c, p_->entries[p_->customId].context);
        break;
    case View::Confirmation:
        if (p_->c.height() < 48) {
            p_->header(p_->modalTitle);
            p_->centered(12, p_->confirmYes ? "YES" : "NO");
        } else {
            p_->header("Confirm");
            p_->centered(17, p_->modalTitle);
            p_->centered(p_->c.height() - 23, p_->confirmYes ? "YES" : "NO");
        }
        p_->footer("BACK", "SELECT", "-/+");
        break;
    case View::Progress: {
        p_->header(p_->modalTitle);
        int16_t w = p_->c.width() - 12, y = p_->c.height() < 48 ? 12 : p_->c.height() / 2;
        p_->c.rect(6, y, w, 8, true);
        p_->c.fillRect(8, y + 2, (int16_t)((w - 4) * p_->fraction), 4, true);
        p_->footer("BACK", "", "WAIT");
        break;
    }
    case View::Splash: {
        int16_t size = minimum<int16_t>(p_->c.width() - 4,
                                        minimum<int16_t>(p_->theme.iconSize, p_->c.height() - 17));
        p_->drawIcon(p_->splashIcon, (p_->c.width() - size) / 2, 1, size, size);
        p_->centered(p_->c.height() - 12, p_->modalTitle);
        break;
    }
    }
    if (p_->message) {
        int16_t y = (p_->c.height() - 22) / 2;
        p_->c.fillRect(1, y, p_->c.width() - 2, 22, false);
        p_->c.roundRect(1, y, p_->c.width() - 2, 22, p_->theme.radius, false, true);
        p_->centered(y + 7, p_->message);
    }
    p_->c.present();
    p_->lastFrame = p_->now;
    p_->dirty = false;
}
void UI::handle(Event event) {
    if (!p_ || !p_->begun || (uint8_t)event > 3)
        return;
    p_->dirty = true;
    if (p_->state == View::Dashboard) {
        Shortcut &s = p_->shortcuts[(uint8_t)event];
        if (s.action) {
            s.action(s.context);
            return;
        }
        if (event == Event::Select)
            openMenu();
        else if (event == Event::Next)
            p_->page = (p_->page + 1) % p_->pages();
        else if (event == Event::Previous)
            p_->page = (p_->page + p_->pages() - 1) % p_->pages();
    } else if (p_->state == View::Menu) {
        uint16_t n = p_->countEntries();
        if (event == Event::Back) {
            if (p_->parent == Root)
                home();
            else
                openMenu(p_->entries[p_->parent].parent);
        } else if (n && event == Event::Next)
            p_->selected = (p_->selected + 1) % n;
        else if (n && event == Event::Previous)
            p_->selected = (p_->selected + n - 1) % n;
        else if (n && event == Event::Select) {
            Id id = p_->entryAt(p_->selected % n);
            Entry e = p_->entries[id];
            if (!e.enabled) {
                notify("LOCKED");
                return;
            }
            if (e.kind == 0)
                e.action(e.context);
            else if (e.kind == 1)
                openMenu(id);
            else if (e.kind == 2)
                openSettings(e.group);
            else {
                p_->customId = id;
                p_->state = View::Custom;
            }
        }
    } else if (p_->state == View::Settings) {
        uint16_t n = p_->countSettings();
        if (event == Event::Back) {
            p_->state = View::Menu;
            p_->selected = 0;
        } else if (n && event == Event::Next)
            p_->selected = (p_->selected + 1) % n;
        else if (n && event == Event::Previous)
            p_->selected = (p_->selected + n - 1) % n;
        else if (n && event == Event::Select)
            edit(p_->settingAt(p_->selected % n));
    } else if (p_->state == View::Editor) {
        Setting &s = p_->settings[p_->editingId];
        if (event == Event::Back)
            p_->state = View::Settings;
        else if (event == Event::Next || event == Event::Previous)
            p_->pending =
                clamp(p_->pending + (event == Event::Next ? s.step : -s.step), s.lo, s.hi);
        else if (event == Event::Select) {
            uint8_t previous[8];
            s.binding.bytes(s.binding.context, previous, false);
            if (!s.binding.write(s.binding.context, p_->pending)) {
                notify("OUT OF RANGE");
                return;
            }
            if (p_->storage && p_->autosave && !save()) {
                s.binding.bytes(s.binding.context, previous, true);
                notify("SAVE FAILED");
                return;
            }
            p_->state = View::Settings;
            if (p_->changed)
                p_->changed(p_->editingId, Change::Edited, p_->changeContext);
        }
    } else if (p_->state == View::Custom) {
        Entry e = p_->entries[p_->customId];
        if ((!e.event || !e.event(event, e.context)) && event == Event::Back)
            p_->state = View::Menu;
    } else if (p_->state == View::Confirmation) {
        if (event == Event::Next || event == Event::Previous)
            p_->confirmYes = !p_->confirmYes;
        else if (event == Event::Select || event == Event::Back) {
            bool yes = event == Event::Select && p_->confirmYes;
            p_->state = p_->returnView;
            if (yes && p_->confirmed)
                p_->confirmed(p_->confirmedContext);
        }
    } else if (event == Event::Back || event == Event::Select)
        home();
}
void UI::home() {
    if (p_) {
        p_->state = View::Dashboard;
        p_->dirty = true;
    }
}
void UI::openMenu(Id parent) {
    if (p_ && p_->validParent(parent)) {
        p_->parent = parent;
        p_->selected = 0;
        p_->state = View::Menu;
        p_->dirty = true;
    }
}
void UI::openSettings(uint8_t group) {
    if (p_) {
        p_->group = group;
        p_->selected = 0;
        p_->state = View::Settings;
        p_->dirty = true;
    }
}
bool UI::edit(Id id) {
    if (!p_ || id >= p_->settings.count)
        return false;
    Setting &s = p_->settings[id];
    if (s.readOnly) {
        notify("READ ONLY");
        return false;
    }
    double value = s.binding.read(s.binding.context);
    p_->pending = finiteNumber(value) ? clamp(value, s.lo, s.hi) : s.lo;
    p_->editingId = id;
    p_->state = View::Editor;
    p_->dirty = true;
    return true;
}
void UI::notify(const char *s, uint32_t duration) {
    if (p_) {
        p_->message = s;
        p_->messageDuration = duration;
        p_->messageAt = p_->now;
        p_->dirty = true;
    }
}
void UI::splash(const char *s, Icon icon, uint32_t duration) {
    if (p_) {
        p_->modalTitle = s;
        p_->splashIcon = icon;
        p_->splashAt = p_->now;
        p_->splashDuration = duration;
        p_->state = View::Splash;
        p_->dirty = true;
    }
}
void UI::progress(const char *s, double f) {
    if (p_) {
        p_->modalTitle = s;
        p_->fraction = finiteNumber(f) ? clamp(f, 0, 1) : 0;
        p_->state = View::Progress;
        p_->dirty = true;
    }
}
void UI::confirm(const char *s, Action yes, void *ctx) {
    if (p_ && s) {
        if (p_->state != View::Confirmation)
            p_->returnView = p_->state;
        p_->modalTitle = s;
        p_->confirmed = yes;
        p_->confirmedContext = ctx;
        p_->confirmYes = false;
        p_->state = View::Confirmation;
        p_->dirty = true;
    }
}
bool UI::attachStorage(Storage &s, size_t offset, size_t bytes, uint32_t schema) {
    if (!p_)
        return false;
    if (offset > s.capacity())
        return p_->fail(Error::InvalidArgument);
    if (!bytes)
        bytes = s.capacity() - offset;
    if (bytes > s.capacity() - offset || bytes / 2 > 65535)
        return p_->fail(Error::InvalidArgument);
    if (bytes < requiredStorageBytes())
        return p_->fail(Error::StorageTooSmall);
    p_->storage = &s;
    p_->storageOffset = offset;
    p_->storageBytes = bytes;
    p_->schema = schema;
    return true;
}
size_t UI::requiredStorageBytes() const {
    return p_ ? 2 * (HeaderBytes + p_->payloadSize()) : 0;
}
LoadResult UI::load() {
    if (!p_ || !p_->storage)
        return LoadResult::NotAttached;
    Bank a = p_->bank(0), b = p_->bank(1), active = p_->newest(a, b);
    if (a.state == 3 || b.state == 3) {
        p_->fail(Error::StorageIO);
        return LoadResult::IOError;
    }
    if (active.state != 1) {
        if (a.state == 2 || b.state == 2) {
            p_->fail(Error::CorruptData);
            return LoadResult::Corrupt;
        }
        return LoadResult::Empty;
    }
    if ((uint32_t)p_->settings.count * 9 + 1 > (size_t)-1) {
        p_->fail(Error::OutOfMemory);
        return LoadResult::OutOfMemory;
    }
    Buffer staged((size_t)p_->settings.count * 9 + 1);
    if (!staged.data) {
        p_->fail(Error::OutOfMemory);
        return LoadResult::OutOfMemory;
    }
    memset(staged.data, 0, (size_t)p_->settings.count * 9 + 1);
    for (Id i = 0; i < p_->settings.count; ++i) {
        Setting &s = p_->settings[i];
        size_t pos = 0;
        for (uint16_t r = 0; r < active.records; ++r) {
            uint8_t h[6];
            for (uint8_t k = 0; k < 6; ++k)
                if (!p_->get(active.base + HeaderBytes + pos + k, h[k])) {
                    p_->fail(Error::StorageIO);
                    return LoadResult::IOError;
                }
            if (read32(h) == s.key && h[4] == s.binding.type && h[5] == s.binding.size) {
                uint8_t value[8];
                for (uint8_t k = 0; k < h[5]; ++k)
                    if (!p_->get(active.base + HeaderBytes + pos + 6 + k, value[k])) {
                        p_->fail(Error::StorageIO);
                        return LoadResult::IOError;
                    }
                if (s.binding.type == 4 && value[0] > 1)
                    break;
                double n = s.binding.decode(value);
                if (finiteNumber(n) && n >= s.lo && n <= s.hi) {
                    staged.data[(size_t)i * 9] = 1;
                    memcpy(staged.data + (size_t)i * 9 + 1, value, s.binding.size);
                }
                break;
            }
            pos += 6 + h[5];
        }
    }
    for (Id i = 0; i < p_->settings.count; ++i)
        if (staged.data[(size_t)i * 9])
            p_->settings[i].binding.bytes(p_->settings[i].binding.context,
                                          staged.data + (size_t)i * 9 + 1, true);
    if (p_->changed)
        for (Id i = 0; i < p_->settings.count; ++i)
            if (staged.data[(size_t)i * 9])
                p_->changed(i, Change::Loaded, p_->changeContext);
    p_->fault = Error::None;
    p_->dirty = true;
    return LoadResult::Loaded;
}
bool UI::save() {
    return p_ && p_->save();
}
bool UI::resetDefaults(bool persist) {
    if (!p_)
        return false;
    if ((uint32_t)p_->settings.count * 8 + 1 > (size_t)-1)
        return p_->fail(Error::OutOfMemory);
    // Back up live values so a failed persistent reset does not change the robot.
    uint8_t *previous = new (std::nothrow) uint8_t[(size_t)p_->settings.count * 8 + 1];
    if (!previous)
        return p_->fail(Error::OutOfMemory);
    for (Id i = 0; i < p_->settings.count; ++i) {
        Setting &s = p_->settings[i];
        s.binding.bytes(s.binding.context, previous + i * 8, false);
        s.binding.bytes(s.binding.context, s.defaults, true);
    }
    bool result = !persist || !p_->storage || save();
    if (!result)
        for (Id i = 0; i < p_->settings.count; ++i)
            p_->settings[i].binding.bytes(p_->settings[i].binding.context, previous + i * 8, true);
    else if (p_->changed)
        for (Id i = 0; i < p_->settings.count; ++i)
            p_->changed(i, Change::Defaults, p_->changeContext);
    delete[] previous;
    p_->dirty = true;
    return result;
}
View UI::view() const {
    return p_ ? p_->state : View::Dashboard;
}
uint16_t UI::widgetCount() const {
    return p_ ? p_->widgets.count : 0;
}
uint16_t UI::settingCount() const {
    return p_ ? p_->settings.count : 0;
}
uint16_t UI::menuCount() const {
    return p_ ? p_->entries.count : 0;
}
uint16_t UI::dashboardPages() const {
    return p_ ? p_->pages() : 0;
}
uint16_t UI::dashboardPage() const {
    return p_ ? p_->page : 0;
}
Id UI::selectedSetting() const {
    return p_ ? p_->state == View::Editor ? p_->editingId : p_->settingAt(p_->selected) : Invalid;
}
Error UI::error() const {
    return p_ ? p_->fault : Error::OutOfMemory;
}
void UI::clearError() {
    if (p_)
        p_->fault = Error::None;
}
const char *UI::errorText() const {
    switch (error()) {
    case Error::None:
        return "OK";
    case Error::OutOfMemory:
        return "Out of memory";
    case Error::InvalidArgument:
        return "Invalid argument";
    case Error::DuplicateKey:
        return "Duplicate setting key";
    case Error::StorageTooSmall:
        return "Storage region too small";
    case Error::StorageIO:
        return "Storage I/O failed";
    case Error::CorruptData:
        return "No valid settings record";
    }
    return "Unknown error";
}
double UI::pendingValue() const {
    return p_ ? p_->pending : 0;
}

ButtonFilter::ButtonFilter(ButtonTiming timing)
    : timing_(timing), raw_(false), stable_(false), repeated_(false), stuck_(false), changed_(0),
      down_(0), lastRepeat_(0) {}
Press ButtonFilter::update(bool pressed, uint32_t now, bool repeat) {
    if (pressed != raw_) {
        raw_ = pressed;
        changed_ = now;
    }
    if (raw_ != stable_ && (uint32_t)(now - changed_) >= timing_.debounceMs) {
        stable_ = raw_;
        if (stable_) {
            down_ = now;
            lastRepeat_ = now;
            repeated_ = false;
            stuck_ = false;
        } else {
            if (stuck_) {
                stuck_ = false;
                return Press::None;
            }
            if (repeated_)
                return Press::None;
            return (uint32_t)(now - down_) >= timing_.holdMs ? Press::Hold : Press::Click;
        }
    }
    if (stable_ && timing_.stuckMs && (uint32_t)(now - down_) >= timing_.stuckMs) {
        stuck_ = true;
        return Press::None;
    }
    if (stable_ && !stuck_ && repeat && (uint32_t)(now - down_) >= timing_.holdMs &&
        (uint32_t)(now - lastRepeat_) >= timing_.repeatMs) {
        repeated_ = true;
        lastRepeat_ = now;
        return Press::Repeat;
    }
    return Press::None;
}

} // namespace indur
