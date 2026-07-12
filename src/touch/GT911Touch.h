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
};
