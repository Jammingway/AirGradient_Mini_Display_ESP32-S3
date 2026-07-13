/**
 * @file AirGradientClient.h
 * Polls the AirGradient Cloud API from a background FreeRTOS task so the
 * UI thread never blocks on HTTP. The latest reading is handed over
 * through a mutex-protected slot; raw JSON never leaves this module.
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../models/AirGradientReading.h"
#include "../settings/SettingsManager.h"  // AppSettings (passed by value/ref)

class SettingsManager;
class WifiManager;

enum class ApiError : uint8_t {
    None,
    NoNetwork,
    Timeout,          // Network category
    AuthFailed,       // Authentication category (401/403)
    BadResponse,      // API category (HTTP != 200 / malformed JSON)
};

class AirGradientClient {
public:
    void begin(SettingsManager& settings, WifiManager& wifi);

    // Thread-safe: copies the latest reading. Returns true if it is valid.
    bool latest(AirGradientReading& out) const;

    ApiError lastError() const { return _lastError; }
    uint32_t lastAttemptMs() const { return _lastAttemptMs; }
    uint32_t consecutiveFailures() const { return _failures; }

    // Host taken from the endpoint (no protocol/path), and its resolved IP
    // when the host is a DNS name. Thread-safe copies for the status screen.
    String host() const;
    String resolvedIp() const;

    // Ask the poll task to fetch immediately (manual refresh).
    void requestNow();

private:
    static void taskEntry(void* arg);
    void taskLoop();
    bool fetchOnce(const AppSettings& s);
    bool parsePayload(class Stream& stream, AirGradientReading& out);
    void setHostInfo(const String& host, const String& ip);

    SettingsManager* _settings = nullptr;
    WifiManager* _wifi = nullptr;
    TaskHandle_t _task = nullptr;
    mutable SemaphoreHandle_t _mutex = nullptr;

    AirGradientReading _reading;         // guarded by _mutex
    String _host;                        // guarded by _mutex
    String _resolvedIp;                  // guarded by _mutex
    volatile ApiError _lastError = ApiError::None;
    volatile uint32_t _lastAttemptMs = 0;
    volatile uint32_t _failures = 0;
};
