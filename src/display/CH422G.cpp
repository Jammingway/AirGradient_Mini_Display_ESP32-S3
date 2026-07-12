#include "CH422G.h"
#include <Wire.h>

bool CH422G::begin() {
    Wire.beginTransmission(REG_WR_SET);
    Wire.write(MODE_IO_OUTPUT);
    _ok = (Wire.endTransmission() == 0);
    if (_ok) {
        _shadow = 0xFF;
        flush();
    }
    return _ok;
}

void CH422G::setPin(uint8_t exio, bool level) {
    if (exio > 7) return;
    if (level) _shadow |= (1u << exio);
    else       _shadow &= ~(1u << exio);
    flush();
}

void CH422G::flush() {
    Wire.beginTransmission(REG_WR_IO);
    Wire.write(_shadow);
    Wire.endTransmission();
}
