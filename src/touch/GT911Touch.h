/**
 * @file GT911Touch.h
 * Polled I2C driver for the GT911 capacitive touch controller.
 * Reset line is wired through the CH422G expander; INT (GPIO4) is left
 * floating during reset so the controller picks its default address.
 */
#pragma once
#include <Arduino.h>

class CH422G;

class GT911Touch {
public:
    bool begin(CH422G& expander);

    // Returns true while a finger is down; x/y in screen coordinates.
    bool read(int16_t& x, int16_t& y);

    uint32_t lastTouchMs() const { return _lastTouchMs; }

private:
    bool readRegs(uint16_t reg, uint8_t* buf, size_t len);
    bool writeReg(uint16_t reg, uint8_t value);

    uint8_t _addr = 0;
    uint32_t _lastTouchMs = 0;

    // Debounce: the GT911 only flags "buffer ready" when it has a fresh
    // report. Between reports a held finger must still count as pressed,
    // otherwise LVGL sees press/release bouncing and drops clicks.
    bool _lastDown = false;
    int16_t _lastX = 0;
    int16_t _lastY = 0;
    uint32_t _lastReportMs = 0;
    static constexpr uint32_t HOLD_TIMEOUT_MS = 100;
};
