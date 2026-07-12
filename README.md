# AirGradient Mini Display

A standalone wireless touchscreen companion display for the **AirGradient ONE**.
Polls the AirGradient Cloud API and renders live environmental metrics on a
dark, configurable dashboard.

**Target hardware:** Waveshare **ESP32-S3-Touch-LCD-4.3**
(ESP32-S3-N16R8 · 800×480 RGB LCD · GT911 capacitive touch · CH422G IO expander)

![splash](src/assets/panda.png)

## Features

- **Boot experience** — anime red-panda splash with the `<syntax_error>` wordmark,
  which blanks into a retro green boot terminal (`>` prompt, blinking `_` cursor)
  streaming live status: WiFi connection, fallback attempts, API polling, errors.
- **Dual SSID** — primary + optional fallback network, automatic failover and
  exponential-backoff reconnect. Neither WiFi loss nor API failure ever crashes
  the UI; cached readings stay on screen with a stale indicator.
- **Dashboard** — JSON-layout-driven widget grid: PM2.5, CO₂, US AQI,
  temperature, humidity, TVOC, NOx, WiFi status, last-updated. Severity-colored
  accent bars (EPA 2024 PM2.5 breakpoints, sensible CO₂/TVOC/NOx thresholds).
- **Touch settings UI** — on-device keyboard; Network / API / General /
  Dashboard tabs; changes persist to NVS immediately. No firmware edit needed
  to configure WiFi or the API token. The API token field has an eye button to
  temporarily reveal the value. Polling interval: 1 / 5 / 10 / 15 min
  (default 5 min). Tapping the "Updated" dashboard card forces a refresh.
- **Themes** — dark (default) and light.
- **Brightness & sleep** — software dimming (the board's backlight is on/off
  only, via the CH422G expander) and an idle sleep timer; any touch wakes it.
- **Non-blocking networking** — HTTP polling runs in a FreeRTOS task on core 0;
  the UI never stalls during requests.

## Building & flashing

Requires [PlatformIO](https://platformio.org/) (uses the
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork of the
espressif32 platform, Arduino core 3.x).

```sh
pio run                 # build
pio run -t upload       # flash over the USB (Type-C next to "USB" silkscreen)
pio device monitor      # 115200 baud logs
```

If the port isn't detected: hold **BOOT**, plug in USB, release BOOT, flash,
then press **RESET**.

## First-time setup

1. Power on — after the splash, the boot terminal reports
   `no network configured`. Tap the screen.
2. **Network tab** — enter your primary SSID/password (fallback SSID optional).
3. **API tab** — the endpoint defaults to
   `https://api.airgradient.com/public/api/v1/locations/measures/current`;
   paste your AirGradient API **token** (AirGradient dashboard → Place →
   Connectivity Settings → API).
4. Tap **← Save**. The device reconnects and loads the dashboard on the first
   successful poll.

## Project layout

```
src/
  boot/       BootManager — app state machine (splash → terminal → dashboard)
  display/    esp_lcd RGB panel driver, CH422G expander, LVGL port
  touch/      GT911 polled I2C driver
  network/    WifiManager — dual-SSID, non-blocking, backoff reconnect
  api/        AirGradientClient — background poll task, ArduinoJson parsing
  settings/   SettingsManager — NVS persistence
  models/     AirGradientReading + US AQI conversion
  themes/     palettes + severity colors
  ui/         SplashScreen, BootTerminal, Dashboard, SettingsScreen
  widgets/    Widget base, MetricWidget (descriptor table), InfoWidget
  assets/     panda.png → panda_img.c (RGB565); panda.svg is an unused
              alternate vector mascot kept for reference
tools/
  png2lvgl.py regenerate the splash C array from a PNG
```

## Notes & known limitations

- HTTPS uses `setInsecure()` (no certificate pinning) — acceptable for read-only
  polling of public sensor data; pin the AirGradient CA if you care.
- The API response's first location entry is displayed. Multi-device support is
  a planned extension (the API module already parses per-location).
- Waking from sleep by touch also delivers that touch to the UI.
- Widget visibility is configurable; free-form drag rearrangement is a
  post-MVP milestone (layout JSON format already supports it).
