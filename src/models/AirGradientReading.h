/**
 * @file AirGradientReading.h
 * Parsed sensor snapshot. Application code never touches raw JSON —
 * only the API module fills this in.
 */
#pragma once
#include <Arduino.h>

struct AirGradientReading {
    float pm25 = NAN;
    float co2 = NAN;
    float temperature = NAN;  // Celsius
    float humidity = NAN;
    float tvoc = NAN;         // TVOC index
    float nox = NAN;          // NOx index
    float aqi = NAN;          // US AQI, derived from pm25
    int   wifiRssi = 0;       // sensor's own WiFi signal
    String deviceName;
    String timestamp;         // ISO string from the API
    uint32_t receivedAtMs = 0;
    bool valid = false;
};

// US EPA AQI from PM2.5 (2024 breakpoints).
inline float aqiFromPm25(float pm) {
    if (isnan(pm) || pm < 0) return NAN;
    struct Bp { float cLo, cHi; int iLo, iHi; };
    static constexpr Bp table[] = {
        {0.0f, 9.0f, 0, 50},
        {9.1f, 35.4f, 51, 100},
        {35.5f, 55.4f, 101, 150},
        {55.5f, 125.4f, 151, 200},
        {125.5f, 225.4f, 201, 300},
        {225.5f, 500.4f, 301, 500},
    };
    for (const auto& bp : table) {
        if (pm <= bp.cHi) {
            float c = max(pm, bp.cLo);
            return bp.iLo + (bp.iHi - bp.iLo) * (c - bp.cLo) / (bp.cHi - bp.cLo);
        }
    }
    return 500;
}
