#pragma once
#include "IndurUI.h"

namespace indur {

// Buffered monochrome Adafruit GFX drivers, including SSD1306 and SH1106G (SH110X).
// Initialize the chosen driver and its bus before calling UI::begin().
template <typename Display> class GfxCanvas : public Canvas {
  public:
    explicit GfxCanvas(Display &display) : display_(display) {}
    int16_t width() const override { return display_.width(); }
    int16_t height() const override { return display_.height(); }
    void clear() override { display_.fillScreen(0); }
    void pixel(int16_t x, int16_t y, bool c) override { display_.drawPixel(x, y, c ? 1 : 0); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool c) override {
        if (w > 0 && h > 0)
            display_.fillRect(x, y, w, h, c ? 1 : 0);
    }
    void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool c) override {
        if (w > 0 && h > 0)
            display_.drawRect(x, y, w, h, c ? 1 : 0);
    }
    void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, bool filled,
                   bool c) override {
        if (w <= 0 || h <= 0)
            return;
        int16_t limit = (w < h ? w : h) / 2;
        if (r > limit)
            r = (uint8_t)limit;
        if (filled)
            display_.fillRoundRect(x, y, w, h, r, c ? 1 : 0);
        else
            display_.drawRoundRect(x, y, w, h, r, c ? 1 : 0);
    }
    void text(int16_t x, int16_t y, const char *s, uint8_t size, bool c) override {
        display_.setFont(0);
        display_.setTextWrap(false);
        display_.setTextSize(size);
        display_.setTextColor(c ? 1 : 0);
        display_.setCursor(x, y);
        display_.print(s ? s : "");
    }
    void present() override { display_.display(); }

  private:
    Display &display_;
};

} // namespace indur
