/**
 * @file SettingsManager.h
 * NVS-backed persistent settings. All fields save immediately on set.
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>

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

    // Dashboard layout (JSON, see Dashboard docs)
    String layoutJson;
};

class SettingsManager {
public:
    static constexpr const char* DEFAULT_ENDPOINT =
        "https://api.airgradient.com/public/api/v1/locations/measures/current";

    bool begin();

    const AppSettings& get() const { return _s; }

    // True when there is enough config to attempt normal operation.
    bool isConfigured() const { return _s.ssid1.length() > 0 && _s.apiKey.length() > 0; }

    void setNetwork(const String& ssid1, const String& pass1,
                    const String& ssid2, const String& pass2);
    void setApi(const String& endpoint, const String& apiKey,
                uint16_t pollIntervalSec, uint16_t timeoutSec,
                uint8_t retryCount, uint16_t retryDelaySec);
    void setGeneral(const String& theme, uint8_t brightness,
                    uint16_t sleepTimeoutMin, bool tempF, bool usAqi);
    void setLayoutJson(const String& layoutJson);

    void factoryReset();

    static const char* defaultLayoutJson();

private:
    void load();

    Preferences _prefs;
    AppSettings _s;
};
