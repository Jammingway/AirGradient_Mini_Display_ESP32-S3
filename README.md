<img width="1024" height="1024" alt="Jammingway-AirQuality" src="https://github.com/user-attachments/assets/9cc2ca1f-7e15-406c-b95b-6785d9cf4b2b" />

# AirGradient Remote Display

A standalone wireless touchscreen companion display for the **AirGradient ONE**.
Polls the sensor's **local server API** over your LAN — no cloud account, no
token — and renders live environmental metrics on a dark, configurable
dashboard.

**Target hardware:** Waveshare **ESP32-S3-Touch-LCD-4.3**
(ESP32-S3-N16R8 · 800×480 RGB LCD · GT911 capacitive touch · CH422G IO expander)

## Features

- **Dual SSID** — primary + optional fallback network, automatic failover and
  exponential-backoff reconnect. Neither WiFi loss nor API failure ever crashes
  the UI; cached readings stay on screen with a stale indicator.
- **Dashboard** — JSON-layout-driven widget grid: PM2.5, CO₂, US AQI,
  temperature, humidity, TVOC, NOx, plus WiFi and System status cards.
  Severity-colored accent bars (EPA 2024 PM2.5 breakpoints, sensible
  CO₂/TVOC/NOx thresholds). Disabling a widget reflows the rest to fill the
  screen evenly — there are never gaps.
- **Trend charts** — tap any metric card for a full-screen history chart over
  1 h / 6 h / 24 h / all; tap again to close. History is a ring buffer in PSRAM
  (4320 samples ≈ 138 KB), downsampled to ≤100 plotted points.
- **Local-first polling** — enter the sensor's base URL or IP; the firmware
  appends `/measures/current`. Configurable interval (15 s / 30 s / 1 / 3 / 5 /
  15 min, default 5 min), request timeout, retry count and retry delay.
- **Touch settings UI** — on-device keyboard; Network / Endpoint / General /
  Dashboard tabs, styled to match the green boot console. Changes persist to
  NVS immediately; no firmware edit needed to configure anything.
- **Sensor naming** — the title shows whatever the sensor reports
  (`locationName` → `model` → `serialno`), overridable via
  *General → Sensor name*.
- **Themes** — dark (default) and light.
- **Brightness & display sleep** — software dimming (the board's backlight is
  on/off only, via the CH422G expander) and an idle sleep timer; any touch
  wakes it.
- **Non-blocking networking** — HTTP polling runs in a FreeRTOS task on core 0;
  the UI never stalls during requests.
- **Debug overlay** — *General → Enable debug overlay* shows reset reason, heap,
  PSRAM, IP/gateway, the exact URL being fetched, and failure counts.

## Building & flashing

Requires [PlatformIO](https://platformio.org/) (uses the
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork of the
espressif32 platform, Arduino core 3.x). LVGL is pinned to **9.3.0**.

```sh
pio run                               # build
pio run -t upload --upload-port COM17 # flash over UART
pio device monitor                    # 115200 baud logs
```

> **Unplug the USB-C connector before flashing.** With USB attached (e.g. as
> backup power) it defeats the DTR/RTS auto-reset, the board never enters the
> bootloader, and the upload hangs indefinitely rather than erroring. With UART
> alone, upload and post-flash reset work normally.

Two serial ports, and they carry different things:

| Port | Carries |
| --- | --- |
| UART0 / CH343 | ROM boot banner, ESP-IDF logs, **panic backtraces** |
| Native USB-CDC | the application's own `Serial` logs (`ARDUINO_USB_CDC_ON_BOOT=1`) |

Decode a panic with
`xtensa-esp-elf-addr2line -pfiaC -e .pio/build/esp32-s3-touch-lcd-43/firmware.elf <addr>`.

## First-time setup

1. Power on — the status terminal reports `no network configured`. Tap anywhere.
2. **Network tab** — enter your primary SSID/password (fallback SSID optional).
3. **Endpoint tab** — enter the base URL or IP of your AirGradient sensor on
   the local network (e.g. `http://192.168.1.50` or
   `http://airgradient_<serial>.local`); the firmware appends
   `/measures/current` ([local server API docs](https://github.com/airgradienthq/arduino/blob/master/docs/local-server.md)).
   Requires sensor firmware ≥ 3.0.10. The token field stays **blank for local
   endpoints**; only fill it when pointing at the cloud API
   (`https://api.airgradient.com/public/api/v1/locations/measures/current`).
4. Tap **< SAVE**. The device reconnects and loads the dashboard on the first
   successful poll.

URLs default to `http://`. `https://` is used only when explicitly specified or
when the host is the official AirGradient API — and the token is only ever
appended for cloud endpoints.

## Project layout

```
src/
  boot/       BootManager — app state machine (splash → terminal → dashboard)
  display/    esp_lcd RGB panel driver, CH422G expander, LVGL port
  touch/      GT911 polled I2C driver
  network/    WifiManager — dual-SSID, non-blocking, backoff reconnect
  api/        AirGradientClient — background poll task, ArduinoJson parsing
  history/    HistoryManager — PSRAM ring buffer feeding the trend charts
  settings/   SettingsManager — NVS persistence (namespace "agdisp")
  models/     AirGradientReading + US AQI conversion
  themes/     palettes + severity colors
  ui/         CascadeSplash, WoprGreeting (boot screens), BootTerminal,
              Dashboard, SettingsScreen
  widgets/    Widget base, MetricWidget (descriptor table), InfoWidget
  assets/     panda.png → panda_img.c (RGB565); panda.svg is an unused
              alternate vector mascot kept for reference
tools/
  png2lvgl.py regenerate a splash C array from a PNG
```

## Notes & known limitations

- **The physical RESET button is unreliable — use Settings > General >
  "Restart Device" instead.** GPIO0 (the ESP32-S3's boot-strap pin) is wired
  to `LCD_PIN_D6` on this board. While the display is running, that line is
  actively driven as part of normal rendering, and a hardware EN reset can
  sample it low, dropping into the ROM's UART-download bootloader instead of
  the app (screen stays black forever). Confirmed 100% reproducible on
  hardware, independent of power source (USB, UART, battery). This is
  Waveshare's wiring, not a firmware bug, and there's no firmware fix for a
  hardware EN-pin reset. A software restart (`ESP.restart()`, wired to the
  Settings button) uses a different reset path — every crash-triggered reboot
  this project has seen has landed back in the app, never in download mode.
- **Pixel clock is 12 MHz, not 16.** The higher rate left faint edge ghosting
  near high-contrast text; 12 MHz (~29 Hz refresh) is flicker-free on this LCD
  and eliminates it. Don't "optimize" it back up. 10 MHz was also tried:
  the panel never locks (solid white from power-on, degrading through an
  argyle pattern to black on reset) — below the ST7262's usable pixel-clock
  range, not a bandwidth symptom. 12 MHz sits in a narrow window with
  measured failures on both sides.
- **Declare `WiFiClient`/`WiFiClientSecure` before `HTTPClient`.**
  `HTTPClient` keeps a raw pointer to the client, and locals are destroyed in
  reverse declaration order — the other order gives `~HTTPClient()` a dangling
  pointer, tears down a socket lwip still owns, and aborts the tcpip thread on
  the next inbound ACK.
- **Never call `WiFi.disconnect(true)`** — `esp_wifi_stop()` leaves lwip on a
  half-torn-down netif. Use `WiFi.disconnect(false)`.
- The API poll task must read settings via `SettingsManager::snapshot()`, never
  `get()` — it runs on another core and a `String` reallocated underneath it
  corrupts the heap.
- Sensor uptime is derived from the local API's `boot` counter (one tick per
  60 s cloud-post cycle), so it is an estimate, shown prefixed with `~`. A
  sensor with cloud posting disabled freezes that counter; the firmware detects
  a stalled value and shows `--` instead of a wrong figure.
- HTTPS uses `setInsecure()` (no certificate pinning) — acceptable for
  read-only polling of public sensor data; pin the AirGradient CA if you care.
- Waking from sleep by touch also delivers that touch to the UI.
- Widget visibility is configurable; free-form drag rearrangement is a
  post-MVP milestone (layout JSON format already supports it).
