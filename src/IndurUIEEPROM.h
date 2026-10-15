#pragma once
#include "IndurUI.h"

namespace indur {

// The application owns initialization (EEPROM.begin on ESP32).
// UI byte accesses stay within attachStorage's region. The backend owns erase granularity.
template <typename Eeprom> class EEPROMStorage : public Storage {
  public:
    EEPROMStorage(Eeprom &eeprom, size_t capacity) : eeprom_(eeprom), capacity_(capacity) {
        size_t actual = length(eeprom, 0);
        if (actual < capacity_)
            capacity_ = actual;
    }
    size_t capacity() const override { return capacity_; }
    bool read(size_t address, uint8_t &value) override {
        if (address >= capacity_)
            return false;
        value = eeprom_.read(address);
        return true;
    }
    bool write(size_t address, uint8_t value) override {
        if (address >= capacity_)
            return false;
        if (eeprom_.read(address) != value)
            eeprom_.write(address, value);
        return true;
    }
    bool flush() override { return commit(eeprom_, 0); }

  private:
    template <typename T> static auto commit(T &e, int) -> decltype(bool(e.commit())) {
        return e.commit();
    }
    template <typename T> static auto commit(T &e, long) -> decltype(e.commit(), bool()) {
        e.commit();
        return true;
    }
    template <typename T> static bool commit(T &, ...) { return true; }
    template <typename T> static auto length(T &e, int) -> decltype(size_t(e.length())) {
        return e.length();
    }
    template <typename T> static size_t length(T &, long) { return (size_t)-1; }
    Eeprom &eeprom_;
    size_t capacity_;
};

} // namespace indur
