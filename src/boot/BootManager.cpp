#include "BootManager.h"
#include "../settings/SettingsManager.h"
#include "../network/WifiManager.h"
#include "../api/AirGradientClient.h"
#include "../themes/ThemeManager.h"
#include "../display/LvglPort.h"
#include "../display/DisplayDriver.h"
#include "../touch/GT911Touch.h"
#include "../utils/Logger.h"

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

    _state = State::Splash;
    _splash.show(theme, [this]() { enterTerminal(); });
}

void BootManager::enterTerminal() {
    _state = State::Terminal;
    _terminal.load();
    if (!_settings->isConfigured()) {
        _terminal.pushLine(_settings->get().apiKey.length() == 0 && _settings->get().ssid1.length() > 0
                               ? "no api token set"
                               : "no network configured");
        _terminal.showConfigHint(true);
    }
}

void BootManager::enterDashboard() {
    if (!_dashboardCreated) {
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
        _terminal.pushLine("settings updated — reconnecting");
        _announcedConnect = false;
        _wifi->restart();
    }
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

    switch (_state) {
        case State::Splash:
        case State::Settings:
            break;

        case State::Terminal: {
            if (_wifi->state() == WifiManager::State::Connected && !_announcedConnect) {
                _announcedConnect = true;
                _terminal.pushLine("polling airgradient api …");
                _terminal.showConfigHint(false);
                _api->requestNow();
            }
            AirGradientReading r;
            if (_api->latest(r)) {
                _terminal.pushLine("data received — loading dashboard");
                enterDashboard();
                break;
            }
            // Surface API failures while still on the terminal.
            static ApiError lastShown = ApiError::None;
            ApiError err = _api->lastError();
            if (err != lastShown && err != ApiError::None && err != ApiError::NoNetwork) {
                lastShown = err;
                switch (err) {
                    case ApiError::AuthFailed:
                        _terminal.pushLine("api auth failed — check token");
                        _terminal.showConfigHint(true);
                        break;
                    case ApiError::Timeout:
                        _terminal.pushLine("api timeout — retrying");
                        break;
                    default:
                        _terminal.pushLine("api error — retrying");
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
    if (!wifiOk) headline = "offline — showing cached data";
    else if (_api->lastError() == ApiError::AuthFailed) headline = "API auth failed — check token";
    else if (_api->consecutiveFailures() > 1) headline = "API unreachable — showing cached data";

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

String BootManager::updatedAgoText(uint32_t receivedAtMs) const {
    uint32_t d = (millis() - receivedAtMs) / 1000;
    if (d < 60) return String(d) + " s ago";
    if (d < 3600) return String(d / 60) + " min ago";
    return String(d / 3600) + " h ago";
}
