#pragma once
#include <Arduino.h>
#include "IndurUI.h"

namespace indur {

typedef bool (*ButtonHandler)(uint8_t button, Press press, void *context);

class Buttons {
  public:
    Buttons(UI &ui, int16_t left, int16_t right, int16_t center = -1,
            ButtonTiming timing = ButtonTiming(), bool activeLow = true, bool pullups = true)
        : ui_(ui), activeLow_(activeLow), pullups_(pullups), hints_(true), faults_(0),
          shownHolds_(0), handler_(0), context_(0) {
        pins_[0] = left;
        pins_[1] = right;
        pins_[2] = center;
        for (uint8_t i = 0; i < 3; ++i)
            filters_[i] = ButtonFilter(timing);
    }
    void onPress(ButtonHandler handler, void *context = 0) {
        handler_ = handler;
        context_ = context;
    }
    void showHoldHints(bool enabled) { hints_ = enabled; }
    void begin() {
        ui_.setThreeButtons(pins_[2] >= 0);
        for (uint8_t i = 0; i < 3; ++i)
            if (pins_[i] >= 0)
                pinMode(pins_[i], pullups_ && activeLow_ ? INPUT_PULLUP : INPUT);
    }
    void update(uint32_t now) {
        uint8_t faults = 0;
        for (uint8_t i = 0; i < 3; ++i) {
            if (pins_[i] < 0)
                continue;
            bool pressed = activeLow_ ? !digitalRead(pins_[i]) : !!digitalRead(pins_[i]);
            bool repeat = pins_[2] >= 0 && i < 2 && ui_.editing();
            Press p = filters_[i].update(pressed, now, repeat);
            uint8_t bit = 1 << i;
            bool held = filters_[i].held(now);
            if (hints_ && held && !repeat && !(shownHolds_ & bit) && !handler_) {
                ui_.notify(i == 1 ? "RELEASE: SELECT" : "RELEASE: BACK", 60000);
                shownHolds_ |= bit;
            }
            if (!held && (shownHolds_ & bit)) {
                shownHolds_ &= ~bit;
                ui_.notify(0, 0);
            }
            if (filters_[i].stuck())
                faults |= (1 << i);
            if (p == Press::None)
                continue;
            if (handler_ && handler_(i, p, context_))
                continue;
            if (i == 2)
                ui_.handle(p == Press::Hold ? Event::Back : Event::Select);
            else if (p == Press::Hold)
                ui_.handle(i == 0 ? Event::Back : Event::Select);
            else
                ui_.handle(i == 0 ? Event::Previous : Event::Next);
        }
        if (faults && faults != faults_)
            ui_.notify("BUTTON STUCK", 1800);
        faults_ = faults;
    }
    uint8_t faultMask() const { return faults_; }

  private:
    UI &ui_;
    int16_t pins_[3];
    ButtonFilter filters_[3];
    bool activeLow_, pullups_, hints_;
    uint8_t faults_, shownHolds_;
    ButtonHandler handler_;
    void *context_;
};

} // namespace indur
