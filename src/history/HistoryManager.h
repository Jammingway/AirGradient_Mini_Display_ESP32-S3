/**
 * @file HistoryManager.h
 * Fixed-capacity ring buffer of sensor readings, held in PSRAM, feeding the
 * on-device trend charts. One timestamped record per successful poll; all
 * metrics share the same sample times.
 */
#pragma once
#include <Arduino.h>
#include "../models/AirGradientReading.h"

enum class Metric : uint8_t { Pm25, Co2, Temp, Humidity, Tvoc, Nox, Aqi, Count };

class HistoryManager {
public:
    bool begin();  // allocates the ring in PSRAM
    void add(const AirGradientReading& r);

    // Copies samples for one metric within the last durationMs (0 = all).
    // Output is oldest-first. Down-samples to at most maxOut points.
    // outVals may contain NAN (metric missing that sample). Returns count.
    int query(Metric m, uint32_t durationMs, uint32_t nowMs, int maxOut,
              float* outVals, uint32_t* outAgeMs) const;

    int count() const { return _count; }
    // Age (ms) of the oldest stored sample, for "All" window labeling.
    uint32_t oldestAgeMs(uint32_t nowMs) const;

private:
    struct Rec {
        uint32_t t;
        float v[(int)Metric::Count];
    };

    int oldestIndex() const { return (_count < _cap) ? 0 : _head; }

    Rec* _buf = nullptr;
    int _cap = 0;
    int _count = 0;
    int _head = 0;  // next write slot
};
