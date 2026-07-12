/**
 * @file CH422G.h
 * Minimal driver for the CH422G I2C IO expander on the Waveshare
 * ESP32-S3-Touch-LCD-4.3. The chip exposes fixed I2C addresses that act
 * as registers; IO0..IO7 are used for TP_RST / LCD_BL / LCD_RST / SD_CS / USB_SEL.
 */
#pragma once
#include <Arduino.h>

class CH422G {
public:
    bool begin();
    void setPin(uint8_t exio, bool level);
    bool ok() const { return _ok; }

private:
    void flush();

    static constexpr uint8_t REG_WR_SET = 0x24;  // system parameter (mode)
    static constexpr uint8_t REG_WR_IO  = 0x38;  // IO0..IO7 output latch
    static constexpr uint8_t MODE_IO_OUTPUT = 0x01;

    uint8_t _shadow = 0xFF;
    bool _ok = false;
};
