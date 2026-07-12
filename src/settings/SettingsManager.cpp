#include "SettingsManager.h"
#include "../utils/Logger.h"

static constexpr const char* NS = "agdisp";

const char* SettingsManager::defaultLayoutJson() {
    return R"({"layout":[
{"type":"metric","metric":"pm25","x":0,"y":0,"w":1,"h":2},
{"type":"metric","metric":"co2","x":1,"y":0,"w":1,"h":2},
{"type":"metric","metric":"aqi","x":2,"y":0,"w":1,"h":2},
{"type":"metric","metric":"temp","x":0,"y":2,"w":1,"h":1},
{"type":"metric","metric":"humidity","x":1,"y":2,"w":1,"h":1},
{"type":"metric","metric":"tvoc","x":2,"y":2,"w":1,"h":1},
{"type":"metric","metric":"nox","x":0,"y":3,"w":1,"h":1},
{"type":"wifi","x":1,"y":3,"w":1,"h":1},
{"type":"updated","x":2,"y":3,"w":1,"h":1}
]})";
}

bool SettingsManager::begin() {
    if (!_prefs.begin(NS, false)) {
        LOG_E("settings", "NVS namespace open failed");
        return false;
    }
    load();
    return true;
}

void SettingsManager::load() {
    _s.ssid1 = _prefs.getString("ssid1", "");
    _s.pass1 = _prefs.getString("pass1", "");
    _s.ssid2 = _prefs.getString("ssid2", "");
    _s.pass2 = _prefs.getString("pass2", "");
    _s.endpoint = _prefs.getString("endpt", DEFAULT_ENDPOINT);
    _s.apiKey = _prefs.getString("apikey", "");
    _s.pollIntervalSec = _prefs.getUShort("poll", 300);
    _s.timeoutSec = _prefs.getUShort("timeout", 10);
    _s.theme = _prefs.getString("theme", "dark");
    _s.brightness = _prefs.getUChar("bright", 100);
    _s.sleepTimeoutMin = _prefs.getUShort("sleep", 0);
    _s.tempFahrenheit = _prefs.getBool("tempF", false);
    _s.usAqi = _prefs.getBool("usAqi", true);
    _s.layoutJson = _prefs.getString("layout", defaultLayoutJson());
    LOG_I("settings", "loaded (configured=%s)", isConfigured() ? "yes" : "no");
}

void SettingsManager::setNetwork(const String& ssid1, const String& pass1,
                                 const String& ssid2, const String& pass2) {
    _s.ssid1 = ssid1; _s.pass1 = pass1;
    _s.ssid2 = ssid2; _s.pass2 = pass2;
    _prefs.putString("ssid1", ssid1);
    _prefs.putString("pass1", pass1);
    _prefs.putString("ssid2", ssid2);
    _prefs.putString("pass2", pass2);
    LOG_I("settings", "network saved (ssid1='%s' ssid2='%s')", ssid1.c_str(), ssid2.c_str());
}

void SettingsManager::setApi(const String& endpoint, const String& apiKey,
                             uint16_t pollIntervalSec, uint16_t timeoutSec) {
    _s.endpoint = endpoint.length() ? endpoint : DEFAULT_ENDPOINT;
    _s.apiKey = apiKey;
    _s.pollIntervalSec = max<uint16_t>(pollIntervalSec, 15);
    _s.timeoutSec = constrain(timeoutSec, (uint16_t)3, (uint16_t)60);
    _prefs.putString("endpt", _s.endpoint);
    _prefs.putString("apikey", _s.apiKey);
    _prefs.putUShort("poll", _s.pollIntervalSec);
    _prefs.putUShort("timeout", _s.timeoutSec);
    LOG_I("settings", "api saved (endpoint='%s')", _s.endpoint.c_str());
}

void SettingsManager::setGeneral(const String& theme, uint8_t brightness,
                                 uint16_t sleepTimeoutMin, bool tempF, bool usAqi) {
    _s.theme = theme;
    _s.brightness = constrain(brightness, (uint8_t)20, (uint8_t)100);
    _s.sleepTimeoutMin = sleepTimeoutMin;
    _s.tempFahrenheit = tempF;
    _s.usAqi = usAqi;
    _prefs.putString("theme", theme);
    _prefs.putUChar("bright", _s.brightness);
    _prefs.putUShort("sleep", sleepTimeoutMin);
    _prefs.putBool("tempF", tempF);
    _prefs.putBool("usAqi", usAqi);
}

void SettingsManager::setLayoutJson(const String& layoutJson) {
    _s.layoutJson = layoutJson;
    _prefs.putString("layout", layoutJson);
}

void SettingsManager::factoryReset() {
    _prefs.clear();
    load();
    LOG_W("settings", "factory reset complete");
}
