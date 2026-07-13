#include "BootManager.h"
#include <WiFi.h>
#include <esp_system.h>
#include "../settings/SettingsManager.h"
#include "../network/WifiManager.h"
#include "../api/AirGradientClient.h"
#include "../themes/ThemeManager.h"
#include "../display/LvglPort.h"
#include "../display/DisplayDriver.h"
#include "../touch/GT911Touch.h"
#include "../utils/Logger.h"

static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "ext-reset";
        case ESP_RST_SW:        return "sw-reset";
        case ESP_RST_PANIC:     return "PANIC (crash)";
        case ESP_RST_INT_WDT:   return "int-watchdog";
        case ESP_RST_TASK_WDT:  return "task-watchdog";
        case ESP_RST_WDT:       return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (power)";
        case ESP_RST_SDIO:      return "sdio";
        default:                return "unknown";
    }
}

void BootManager::begin(SettingsManager& settings, WifiManager& wifi, AirGradientClient& api,
                        ThemeManager& theme, LvglPort& lvgl, DisplayDriver& display,
                        GT911Touch& touch) {
    _settings = &settings;
    _wifi = &wifi;
    _api = &api;
    _theme = &theme;
    _lvgl = &lvgl;
    _display = &display;
    _touch = &touch;

    _resetReason = resetReasonStr(esp_reset_reason());
    LOG_I("boot", "last reset reason: %s", _resetReason);

    _history.begin();  // PSRAM ring for trend charts

    // Terminal exists from the start so early WiFi events land in it.
    _terminal.create(theme);
    _terminal.pushLine("display init .......... OK");
    _terminal.pushLine("settings loaded ....... OK");
    _terminal.onTap([this]() {
        if (_state == State::Terminal) openSettings();
    });

    _wifi->onStatus([this](const String& msg) {
        _terminal.pushLine("wifi: " + msg);
        bool stalled = _wifi->state() == WifiManager::State::Idle ||
                       _wifi->state() == WifiManager::State::WaitingRetry;
        _terminal.showConfigHint(stalled || !_settings->isConfigured());
    });

    // The splash can be skipped from Settings -> General.
    if (_settings->get().disableSplash) {
        enterTerminal();
    } else {
        _state = State::Splash;
        // deletePrev=true frees the splash (and all its cascade objects)
        // once the transition completes.
        _splash.show(theme, [this]() { enterTerminal(true); });
    }
}

void BootManager::enterTerminal(bool deletePrev) {
    _state = State::Terminal;
    _terminal.load(deletePrev);

    // Start networking now (once), after the splash — separating the WiFi
    // inrush from the boot animation's render current to avoid brownout.
    if (!_netStarted) {
        _netStarted = true;
        _wifi->begin(*_settings);
        _api->begin(*_settings, *_wifi);
    }

    if (!_settings->isConfigured()) {
        _terminal.pushLine(_settings->get().endpoint.length() == 0 && _settings->get().ssid1.length() > 0
                               ? "no endpoint set"
                               : "no network configured");
        _terminal.showConfigHint(true);
    }
}

void BootManager::enterDashboard() {
    if (!_dashboardCreated) {
        _dashboard.setHistory(&_history);
        _dashboard.create(*_theme, *_settings);
        _dashboard.onRefresh([this]() { _api->requestNow(); });
        _dashboard.onSettings([this]() { openSettings(); });
        _dashboardCreated = true;
    }
    AirGradientReading r;
    _api->latest(r);
    _dashboard.updateReading(r, _settings->get(), *_theme);
    updateDashboardStatus();
    _dashboard.load();
    _state = State::Dashboard;
}

void BootManager::openSettings() {
    if (_settingsScreen.isOpen()) return;
    _stateBeforeSettings = _state;
    _state = State::Settings;
    _settingsScreen.open(*_settings, *_theme,
                         [this](const SettingsScreen::Result& res) { onSettingsClosed(res); });
}

void BootManager::onSettingsClosed(const SettingsScreen::Result& res) {
    const AppSettings& s = _settings->get();
    _theme->apply(s.theme);
    _lvgl->setBrightness(s.brightness);

    bool backToTerminal = res.networkChanged || res.factoryReset ||
                          _stateBeforeSettings == State::Terminal;

    if (_dashboardCreated && (res.generalChanged || res.layoutChanged || res.factoryReset)) {
        // Cheapest correct path for theme/layout changes: rebuild widgets.
        _dashboard.rebuild(*_theme, *_settings);
    }

    if (res.networkChanged || res.factoryReset) {
        _terminal.pushLine("settings updated - reconnecting");
        _announcedConnect = false;
        _announcedTarget = false;
        _wifi->restart();
    }
    if (res.apiChanged) _announcedTarget = false;  // re-announce new target
    if (res.apiChanged) _api->requestNow();

    // deletePrev=true: LVGL frees the settings screen after the transition.
    if (backToTerminal) {
        _state = State::Terminal;
        _terminal.load(true);
        _terminal.showConfigHint(!_settings->isConfigured());
    } else {
        AirGradientReading r;
        _api->latest(r);
        _dashboard.updateReading(r, _settings->get(), *_theme);
        updateDashboardStatus();
        _dashboard.load(true);
        _state = State::Dashboard;
    }
}

void BootManager::tick() {
    handleSleep();
    updateDebug();

    // Record every new reading into history (any screen), for the charts.
    {
        AirGradientReading r;
        if (_api->latest(r) && r.valid && r.receivedAtMs != _lastHistoryAt) {
            _lastHistoryAt = r.receivedAtMs;
            _history.add(r);
        }
    }

    switch (_state) {
        case State::Splash:
        case State::Settings:
            break;

        case State::Terminal: {
            if (_wifi->state() == WifiManager::State::Connected && !_announcedConnect) {
                _announcedConnect = true;
                _terminal.pushLine("polling local api ...");
                _terminal.showConfigHint(false);
                _api->requestNow();
            }
            // Once the poll task has computed the target host/IP, show it.
            if (_announcedConnect && !_announcedTarget) {
                String host = _api->host();
                if (host.length()) {
                    _announcedTarget = true;
                    _terminal.pushLine("  target: " + host);
                    String ip = _api->resolvedIp();
                    if (ip.length() && ip != host) {
                        _terminal.pushLine("  resolved: " + ip);
                    }
                }
            }
            AirGradientReading r;
            if (_api->latest(r)) {
                _terminal.pushLine("data received - loading dashboard");
                enterDashboard();
                break;
            }
            // Surface API failures (with the real transport reason) while
            // still on the terminal; re-show when the reason text changes.
            static ApiError lastShown = ApiError::None;
            static String lastDetail;
            ApiError err = _api->lastError();
            String detail = _api->lastErrorText();
            if ((err != lastShown || detail != lastDetail) &&
                err != ApiError::None && err != ApiError::NoNetwork) {
                lastShown = err;
                lastDetail = detail;
                switch (err) {
                    case ApiError::AuthFailed:
                        _terminal.pushLine("auth failed - check token");
                        _terminal.showConfigHint(true);
                        break;
                    case ApiError::Timeout:
                        _terminal.pushLine("cannot reach sensor: " +
                                           (detail.length() ? detail : String("timeout")));
                        break;
                    default:
                        _terminal.pushLine("api error: " +
                                           (detail.length() ? detail : String("bad response")));
                        break;
                }
            }
            break;
        }

        case State::Dashboard: {
            AirGradientReading r;
            if (_api->latest(r) && r.receivedAtMs != _lastReceivedAt) {
                _lastReceivedAt = r.receivedAtMs;
                _dashboard.updateReading(r, _settings->get(), *_theme);
            }
            if (millis() - _lastStatusTickMs >= 1000) {
                _lastStatusTickMs = millis();
                updateDashboardStatus();
            }
            break;
        }
    }
}

void BootManager::updateDashboardStatus() {
    AirGradientReading r;
    bool hasData = _api->latest(r);

    bool wifiOk = _wifi->isConnected();
    String updated = hasData ? updatedAgoText(r.receivedAtMs) : String("no data");
    uint32_t staleMs = (uint32_t)_settings->get().pollIntervalSec * 3000;
    bool stale = !hasData || (millis() - r.receivedAtMs) > staleMs;

    String headline;
    if (!wifiOk) headline = "offline - showing cached data";
    else if (_api->lastError() == ApiError::AuthFailed) headline = "API auth failed - check token";
    else if (_api->consecutiveFailures() > 1) headline = "API unreachable - showing cached data";

    _dashboard.updateStatus(_wifi->statusText(), !wifiOk, updated, stale, headline, *_theme);
}

void BootManager::handleSleep() {
    uint16_t timeoutMin = _settings->get().sleepTimeoutMin;
    if (timeoutMin == 0) {
        if (!_display->backlightOn()) _display->setBacklight(true);
        return;
    }
    uint32_t idleMs = millis() - _touch->lastTouchMs();
    if (_display->backlightOn() && idleMs > (uint32_t)timeoutMin * 60000UL) {
        LOG_I("sleep", "idle %u min — backlight off", timeoutMin);
        _display->setBacklight(false);
    } else if (!_display->backlightOn() && idleMs < 1000) {
        LOG_I("sleep", "touch — backlight on");
        _display->setBacklight(true);
    }
}

void BootManager::updateDebug() {
    static bool wasEnabled = false;
    bool enabled = _settings->get().debug;

    if (!enabled) {
        if (wasEnabled) {  // just turned off: clear overlays once
            _terminal.setDebugLine("");
            _dashboard.setDebugLine("");
            wasEnabled = false;
        }
        return;
    }
    wasEnabled = true;

    if (millis() - _lastDebugTickMs < 1000) return;
    _lastDebugTickMs = millis();

    String detail = _api->lastErrorText();  // real transport reason
    String url = _api->lastUrl();           // exact URL being requested
    String sensor = _api->resolvedIp();     // sensor IP (resolved or literal)
    if (sensor.length() == 0) sensor = _api->host();

    // The display's own network identity — compare the subnet against the
    // sensor's to catch a wrong-SSID / VLAN / isolation mismatch, which is
    // the usual cause of "connection refused" from a same-subnet host.
    String ssid = WiFi.SSID();
    String me   = WiFi.localIP().toString();
    String gw   = WiFi.gatewayIP().toString();
    String mask = WiFi.subnetMask().toString();
    String dns  = WiFi.dnsIP().toString();

    char buf[480];
    snprintf(buf, sizeof(buf),
             "rst=%s heap=%uk min=%uk psram=%uk\n"
             "ssid=%s rssi=%d  me=%s gw=%s\n"
             "mask=%s dns=%s\n"
             "GET %s\n"
             "sensor=%s fail=%u err=%s",
             _resetReason,
             (unsigned)(ESP.getFreeHeap() / 1024),
             (unsigned)(ESP.getMinFreeHeap() / 1024),
             (unsigned)(ESP.getFreePsram() / 1024),
             ssid.length() ? ssid.c_str() : "-", _wifi->rssi(),
             me.c_str(), gw.c_str(),
             mask.c_str(), dns.c_str(),
             url.length() ? url.c_str() : "(none yet)",
             sensor.length() ? sensor.c_str() : "-",
             (unsigned)_api->consecutiveFailures(),
             detail.length() ? detail.c_str() : "-");

    _terminal.setDebugLine(buf);
    _dashboard.setDebugLine(buf);
}

String BootManager::updatedAgoText(uint32_t receivedAtMs) const {
    uint32_t d = (millis() - receivedAtMs) / 1000;
    if (d < 60) return String(d) + " s ago";
    if (d < 3600) return String(d / 60) + " min ago";
    return String(d / 3600) + " h ago";
}
