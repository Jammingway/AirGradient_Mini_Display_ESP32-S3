#include "GT911Touch.h"
#include <Wire.h>
#include "board_pins.h"
#include "../display/CH422G.h"
#include "../utils/Logger.h"

static constexpr uint16_t REG_STATUS = 0x814E;
static constexpr uint16_t REG_POINT0 = 0x8150;

bool GT911Touch::begin(CH422G& expander) {
    // Hardware reset via expander, INT floating -> default address.
    pinMode(TOUCH_PIN_INT, INPUT);
    expander.setPin(EXIO_TP_RST, false);
    delay(20);
    expander.setPin(EXIO_TP_RST, true);
    delay(120);

    for (uint8_t addr : {TOUCH_I2C_ADDR_PRIMARY, TOUCH_I2C_ADDR_ALT}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            _addr = addr;
            break;
        }
    }
    if (_addr == 0) {
        LOG_E("touch", "GT911 not found on I2C bus");
        return false;
    }

    uint8_t id[4] = {0};
    readRegs(0x8140, id, 4);
    LOG_I("touch", "GT911 at 0x%02X, product id: %c%c%c", _addr, id[0], id[1], id[2]);
    return true;
}

bool GT911Touch::read(int16_t& x, int16_t& y) {
    uint8_t status = 0;
    bool i2cOk = readRegs(REG_STATUS, &status, 1);

    if (!i2cOk || !(status & 0x80)) {
        // No fresh report. The GT911 reports at ~100 Hz while touched, so a
        // short gap means "state unchanged" — keep reporting the held press
        // instead of bouncing to released mid-click.
        if (_lastDown && millis() - _lastReportMs < HOLD_TIMEOUT_MS) {
            x = _lastX;
            y = _lastY;
            return true;
        }
        _lastDown = false;
        return false;
    }

    uint8_t touches = status & 0x0F;
    bool down = false;
    if (touches >= 1) {
        // Point 1 layout at 0x8150: xL xH yL yH
        uint8_t raw[4];
        if (readRegs(REG_POINT0, raw, 4)) {
            x = (int16_t)(raw[0] | (raw[1] << 8));
            y = (int16_t)(raw[2] | (raw[3] << 8));
            if (x >= 0 && x < LCD_H_RES && y >= 0 && y < LCD_V_RES) {
                down = true;
                _lastTouchMs = millis();
                _lastX = x;
                _lastY = y;
            }
        }
    }
    writeReg(REG_STATUS, 0);  // ack: clear buffer status
    _lastDown = down;
    _lastReportMs = millis();
    return down;
}

bool GT911Touch::readRegs(uint16_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)_addr, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

bool GT911Touch::writeReg(uint16_t reg, uint8_t value) {
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
