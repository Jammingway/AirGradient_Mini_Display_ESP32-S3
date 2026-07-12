/**
 * @file WifiManager.h
 * Non-blocking station-mode WiFi with primary + optional fallback SSID,
 * timed attempts and exponential-backoff reconnect. Ticked from loop();
 * never blocks the UI.
 */
#pragma once
#include <Arduino.h>
#include <functional>

class SettingsManager;

class WifiManager {
public:
    enum class State : uint8_t {
        Idle,          // nothing configured
        Connecting,
        Connected,
        WaitingRetry,  // both SSIDs failed; backing off
    };

    using StatusCallback = std::function<void(const String& message)>;

    void begin(SettingsManager& settings);
    void tick();

    // Drop the current connection and start over (e.g. after settings change).
    void restart();

    State state() const { return _state; }
    bool isConnected() const { return _connectedFlag; }
    String statusText() const;
    int rssi() const;
    String currentSsid() const { return _currentSsid; }

    void onStatus(StatusCallback cb) { _statusCb = std::move(cb); }

private:
    void startAttempt(uint8_t index);
    void enterRetryWait();
    void notify(const String& msg);

    SettingsManager* _settings = nullptr;
    State _state = State::Idle;
    uint8_t _attemptIndex = 0;      // 0 = primary, 1 = fallback
    uint8_t _retryCount = 0;
    uint32_t _stateSinceMs = 0;
    uint32_t _retryDelayMs = 0;
    String _currentSsid;
    volatile bool _connectedFlag = false;  // read from the API task
    StatusCallback _statusCb;

    static constexpr uint32_t ATTEMPT_TIMEOUT_MS = 15000;
};
