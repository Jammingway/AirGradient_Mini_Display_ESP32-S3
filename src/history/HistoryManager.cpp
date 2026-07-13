#include "HistoryManager.h"
#include "esp_heap_caps.h"
#include "../utils/Logger.h"

// 4320 records x 32 bytes = ~138 KB in PSRAM. At a 5-min poll that's ~15
// days of history; at 30 s, ~36 h; at 15 s, ~18 h.
static constexpr int CAPACITY = 4320;

bool HistoryManager::begin() {
    _buf = (Rec*)heap_caps_malloc(sizeof(Rec) * CAPACITY, MALLOC_CAP_SPIRAM);
    if (!_buf) {
        LOG_E("history", "PSRAM alloc failed (%u bytes)", (unsigned)(sizeof(Rec) * CAPACITY));
        return false;
    }
    _cap = CAPACITY;
    _count = 0;
    _head = 0;
    LOG_I("history", "ring ready: %d records (%u KB PSRAM)",
          _cap, (unsigned)(sizeof(Rec) * CAPACITY / 1024));
    return true;
}

void HistoryManager::add(const AirGradientReading& r) {
    if (!_buf) return;
    Rec& rec = _buf[_head];
    rec.t = r.receivedAtMs ? r.receivedAtMs : millis();
    rec.v[(int)Metric::Pm25] = r.pm25;
    rec.v[(int)Metric::Co2] = r.co2;
    rec.v[(int)Metric::Temp] = r.temperature;
    rec.v[(int)Metric::Humidity] = r.humidity;
    rec.v[(int)Metric::Tvoc] = r.tvoc;
    rec.v[(int)Metric::Nox] = r.nox;
    rec.v[(int)Metric::Aqi] = r.aqi;

    _head = (_head + 1) % _cap;
    if (_count < _cap) _count++;
}

int HistoryManager::query(Metric m, uint32_t durationMs, uint32_t nowMs, int maxOut,
                          float* outVals, uint32_t* outAgeMs) const {
    if (!_buf || _count == 0 || maxOut <= 0) return 0;
    int mi = (int)m;
    int start = oldestIndex();

    auto inWindow = [&](uint32_t t) {
        return durationMs == 0 || (uint32_t)(nowMs - t) <= durationMs;
    };

    int matches = 0;
    for (int i = 0; i < _count; i++) {
        if (inWindow(_buf[(start + i) % _cap].t)) matches++;
    }
    if (matches == 0) return 0;

    int stride = (matches + maxOut - 1) / maxOut;
    if (stride < 1) stride = 1;

    int out = 0, seen = 0, lastMatchIdx = -1;
    for (int i = 0; i < _count; i++) {
        int idx = (start + i) % _cap;
        uint32_t t = _buf[idx].t;
        if (!inWindow(t)) continue;
        lastMatchIdx = idx;
        if (seen % stride == 0 && out < maxOut) {
            outVals[out] = _buf[idx].v[mi];
            outAgeMs[out] = (uint32_t)(nowMs - t);
            out++;
        }
        seen++;
    }
    // Always include the newest sample so the line ends at "now".
    if (lastMatchIdx >= 0 && out > 0 &&
        outAgeMs[out - 1] != (uint32_t)(nowMs - _buf[lastMatchIdx].t)) {
        if (out < maxOut) out++;
        outVals[out - 1] = _buf[lastMatchIdx].v[mi];
        outAgeMs[out - 1] = (uint32_t)(nowMs - _buf[lastMatchIdx].t);
    }
    return out;
}

uint32_t HistoryManager::oldestAgeMs(uint32_t nowMs) const {
    if (!_buf || _count == 0) return 0;
    return (uint32_t)(nowMs - _buf[oldestIndex()].t);
}
