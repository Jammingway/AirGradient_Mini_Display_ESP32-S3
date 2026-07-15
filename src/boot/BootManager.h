/**
 * @file BootManager.h
 * Application state machine:
 *   Splash -> BootTerminal -> Dashboard (<-> Settings)
 * Failure paths never leave the user without feedback: the terminal shows
 * WiFi/API status and offers tap-to-configure whenever progress stalls.
 */
#pragma once
#include <Arduino.h>
#include "../ui/WoprGreeting.h"
#include "../ui/BootTerminal.h"
#include "../ui/Dashboard.h"
#include "../ui/SettingsScreen.h"
#include "../history/HistoryManager.h"

class SettingsManager;
class WifiManager;
class AirGradientClient;
class ThemeManager;
class LvglPort;
class DisplayDriver;
class GT911Touch;

class BootManager {
public:
    void begin(SettingsManager& settings, WifiManager& wifi, AirGradientClient& api,
               ThemeManager& theme, LvglPort& lvgl, DisplayDriver& display,
               GT911Touch& touch);
    void tick();

private:
    enum class State : uint8_t { Splash, Terminal, Dashboard, Settings };

    void enterTerminal(bool deletePrev = false);
    void enterDashboard();
    void openSettings();
    void onSettingsClosed(const SettingsScreen::Result& res);
    void updateDashboardStatus();
    void handleSleep();
    void updateDebug();
    String updatedAgoText(uint32_t receivedAtMs) const;
    // Empty when the sensor's boot counter is absent or has stalled.
    String sensorUptimeText(const AirGradientReading& r);

    SettingsManager* _settings = nullptr;
    WifiManager* _wifi = nullptr;
    AirGradientClient* _api = nullptr;
    ThemeManager* _theme = nullptr;
    LvglPort* _lvgl = nullptr;
    DisplayDriver* _display = nullptr;
    GT911Touch* _touch = nullptr;

    WoprGreeting _splash;
    BootTerminal _terminal;
    Dashboard _dashboard;
    SettingsScreen _settingsScreen;
    HistoryManager _history;
    uint32_t _lastHistoryAt = 0;
    bool _dashboardCreated = false;
    bool _netStarted = false;

    State _state = State::Splash;
    State _stateBeforeSettings = State::Terminal;
    bool _announcedConnect = false;
    bool _announcedTarget = false;
    uint32_t _lastReceivedAt = 0;
    uint32_t _lastStatusTickMs = 0;
    uint32_t _lastDebugTickMs = 0;
    uint32_t _lastRepaintMs = 0;
    const char* _resetReason = "";   // captured once at boot

    // Tracks the sensor's boot counter to detect a stalled one (see
    // sensorUptimeText).
    uint32_t _sensorBootMinutes = 0;
    uint32_t _sensorBootAtMs = 0;
    bool _sensorBootSeen = false;
    bool _sensorBootStalled = false;
};
