/**
 * @file DisplayDriver.h
 * ESP32-S3 parallel RGB panel driver (esp_lcd) for the 800x480 ST7262 LCD,
 * plus backlight / reset sequencing through the CH422G expander.
 */
#pragma once
#include <Arduino.h>
#include "esp_lcd_panel_ops.h"
#include "CH422G.h"

class DisplayDriver {
public:
    bool begin();

    void setBacklight(bool on);
    bool backlightOn() const { return _backlightOn; }

    // Blits a rectangle of RGB565 pixels into the panel framebuffer.
    void drawBitmap(int x1, int y1, int x2, int y2, const void* pixels);

    CH422G& expander() { return _expander; }

private:
    esp_lcd_panel_handle_t _panel = nullptr;
    CH422G _expander;
    bool _backlightOn = false;
};
