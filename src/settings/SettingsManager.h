/**
 * @file SettingsManager.h
 * NVS-backed persistent settings. All fields save immediately on set.
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct AppSettings {
    // Network — primary SSID required for operation, fallback optional.
    String ssid1;
    String pass1;
    String ssid2;
    String pass2;

    // API
    String endpoint;      // full URL; {token} handled as query param
    String apiKey;
    uint16_t pollIntervalSec = 300;
    uint16_t timeoutSec = 10;
    uint8_t retryCount = 3;        // attempts per poll cycle (1 = no retry)
    uint16_t retryDelaySec = 10;   // pause between attempts

    // General
    String theme = "dark";
    uint8_t brightness = 100;      // 20..100
    uint16_t sleepTimeoutMin = 0;  // 0 = never
    bool tempFahrenheit = false;
    bool usAqi = true;
    bool disableSplash = false;    // skip the boot splash straight to terminal
    bool debug = false;            // show diagnostics overlay on status/dashboard

    // User-chosen name for the sensor, set by double-tapping the dashboard
    // title. Empty = fall back to whatever the sensor reports.
    String deviceName;

    // Dashboard layout (JSON, see Dashboard docs)
    String layoutJson;
};

class SettingsManager {
public:
    // No default: the endpoint is the AirGradient sensor's base URL or IP
    // on the local network (e.g. "http://192.168.1.50"), entered in setup.
    static constexpr const char* DEFAULT_ENDPOINT = "";

    bool begin();

    // Main (UI) thread only: returns a reference to the live settings.
    const AppSettings& get() const { return _s; }

    // Thread-safe by-value copy. The API poll task MUST use this instead of
    // get(): it runs on another core, and a writer on the UI thread
    // reallocating a String mid-read would otherwise corrupt the heap.
    AppSettings snapshot() const;

    // True when there is enough config to attempt normal operation.
    // Token is optional (only cloud endpoints need it).
    bool isConfigured() const { return _s.ssid1.length() > 0 && _s.endpoint.length() > 0; }

    void setNetwork(const String& ssid1, const String& pass1,
                    const String& ssid2, const String& pass2);
    void setApi(const String& endpoint, const String& apiKey,
                uint16_t pollIntervalSec, uint16_t timeoutSec,
                uint8_t retryCount, uint16_t retryDelaySec);
    void setGeneral(const String& theme, uint8_t brightness,
                    uint16_t sleepTimeoutMin, bool tempF, bool usAqi,
                    bool disableSplash, bool debug);
    void setLayoutJson(const String& layoutJson);
    // Empty string clears the override and restores the reported name.
    void setDeviceName(const String& name);

    void factoryReset();

    static const char* defaultLayoutJson();

private:
    void load();

    Preferences _prefs;
    AppSettings _s;
    // Guards _s against the concurrent API poll task (see snapshot()).
    SemaphoreHandle_t _mutex = nullptr;
};
