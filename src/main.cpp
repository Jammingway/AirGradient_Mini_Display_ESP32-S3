/**
 * AirGradient Mini Display — Waveshare ESP32-S3-Touch-LCD-4.3
 *
 * Standalone touchscreen companion display for the AirGradient ONE:
 * polls the AirGradient Cloud API and renders a configurable dashboard.
 * See README.md for setup and flashing instructions.
 */
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

#include "board_pins.h"
#include "utils/Logger.h"
#include "settings/SettingsManager.h"
#include "display/DisplayDriver.h"
#include "display/LvglPort.h"
#include "touch/GT911Touch.h"
#include "themes/ThemeManager.h"
#include "network/WifiManager.h"
#include "api/AirGradientClient.h"
#include "boot/BootManager.h"

static SettingsManager settings;
static DisplayDriver display;
static GT911Touch touch;
static LvglPort lvglPort;
static ThemeManager theme;
static WifiManager wifi;
static AirGradientClient api;
static BootManager bootManager;

static void fatal(const char* what) {
    // No display or config — blink the log forever rather than crash-loop.
    for (;;) {
        LOG_E("boot", "fatal: %s", what);
        delay(5000);
    }
}

void setup() {
    Serial.begin(115200);
    LOG_I("boot", "AirGradient Mini Display starting");

    Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL, 400000);

    if (!settings.begin()) fatal("settings storage");
    theme.apply(settings.get().theme);

    if (!display.begin()) fatal("display init");
    touch.begin(display.expander());  // touch failure is non-fatal; UI still renders

    if (!lvglPort.begin(display, touch)) fatal("lvgl init");
    lvglPort.setBrightness(settings.get().brightness);

    // UI first: the splash appears while WiFi negotiates in the background.
    bootManager.begin(settings, wifi, api, theme, lvglPort, display, touch);

    // First frame before the backlight turns on, so there is no white flash.
    lv_timer_handler();
    display.setBacklight(true);

    wifi.begin(settings);
    api.begin(settings, wifi);

    LOG_I("boot", "setup complete, free heap: %u, psram: %u",
          ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
    wifi.tick();
    bootManager.tick();
    lv_timer_handler();
    delay(2);
}
