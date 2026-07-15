#include "WifiManager.h"
#include <WiFi.h>
#include "../settings/SettingsManager.h"
#include "../utils/Logger.h"

void WifiManager::begin(SettingsManager& settings) {
    _settings = &settings;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // reconnect policy is ours
    WiFi.persistent(false);        // credentials live in our own NVS namespace
    // Modem power-save adds seconds of latency to the first packets after an
    // idle period — enough to time out the first API poll of each cycle.
    WiFi.setSleep(false);

    if (_settings->get().ssid1.length() > 0) {
        startAttempt(0);
    } else {
        _state = State::Idle;
        notify("no network configured");
    }
}

void WifiManager::restart() {
    // false = keep the radio/driver up (see startAttempt).
    WiFi.disconnect(false);
    _connectedFlag = false;
    _retryCount = 0;
    if (_settings->get().ssid1.length() > 0) {
        startAttempt(0);
    } else {
        _state = State::Idle;
        notify("no network configured");
    }
}

void WifiManager::startAttempt(uint8_t index) {
    const AppSettings& s = _settings->get();
    _attemptIndex = index;
    _currentSsid = (index == 0) ? s.ssid1 : s.ssid2;
    const String& pass = (index == 0) ? s.pass1 : s.pass2;

    // Drop the association only — NEVER disconnect(true). The `wifioff=true`
    // form calls esp_wifi_stop(); restarting it here made the netstack
    // callback re-registration fail (ESP_ERR_WIFI_STOP_STATE) and left lwip
    // attached to a half-torn-down netif, so the next inbound TCP packet
    // aborted in the tcpip thread with "pbuf_free: p->ref > 0".
    WiFi.disconnect(false);
    delay(10);
    WiFi.begin(_currentSsid.c_str(), pass.c_str());

    _state = State::Connecting;
    _stateSinceMs = millis();
    notify("connecting to " + _currentSsid + (index == 1 ? " (fallback)" : ""));
    LOG_I("wifi", "attempt %u: '%s'", index, _currentSsid.c_str());
}

void WifiManager::enterRetryWait() {
    static const uint32_t delays[] = {5000, 10000, 30000, 60000};
    _retryDelayMs = delays[min<uint8_t>(_retryCount, 3)];
    if (_retryCount < 250) _retryCount++;
    _state = State::WaitingRetry;
    _stateSinceMs = millis();
    _connectedFlag = false;
    notify("all networks failed - retrying in " + String(_retryDelayMs / 1000) + "s");
    LOG_W("wifi", "retry in %lu ms", _retryDelayMs);
}

void WifiManager::tick() {
    switch (_state) {
        case State::Idle:
            break;

        case State::Connecting: {
            wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) {
                _state = State::Connected;
                _connectedFlag = true;
                _retryCount = 0;
                notify("ip acquired: " + WiFi.localIP().toString());
                LOG_I("wifi", "connected to '%s', ip=%s, rssi=%d",
                      _currentSsid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
            } else if (millis() - _stateSinceMs > ATTEMPT_TIMEOUT_MS) {
                LOG_W("wifi", "attempt %u timed out", _attemptIndex);
                bool haveFallback = _settings->get().ssid2.length() > 0;
                if (_attemptIndex == 0 && haveFallback) {
                    startAttempt(1);
                } else {
                    enterRetryWait();
                }
            }
            break;
        }

        case State::Connected:
            if (WiFi.status() != WL_CONNECTED) {
                _connectedFlag = false;
                notify("wifi lost - reconnecting");
                LOG_W("wifi", "connection lost");
                startAttempt(0);
            }
            break;

        case State::WaitingRetry:
            if (millis() - _stateSinceMs >= _retryDelayMs) {
                startAttempt(0);
            }
            break;
    }
}

String WifiManager::statusText() const {
    switch (_state) {
        case State::Idle:         return "not configured";
        case State::Connecting:   return "connecting...";
        case State::Connected:    return _currentSsid + " (" + String(WiFi.RSSI()) + " dBm)";
        case State::WaitingRetry: return "offline - retrying";
    }
    return "";
}

int WifiManager::rssi() const {
    return (_state == State::Connected) ? WiFi.RSSI() : 0;
}

void WifiManager::notify(const String& msg) {
    if (_statusCb) _statusCb(msg);
}
