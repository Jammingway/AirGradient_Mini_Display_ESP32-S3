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
    _mutex = xSemaphoreCreateMutex();
    if (!_prefs.begin(NS, false)) {
        LOG_E("settings", "NVS namespace open failed");
        return false;
    }
    load();
    return true;
}

AppSettings SettingsManager::snapshot() const {
    if (!_mutex) return _s;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    AppSettings copy = _s;  // deep-copies Strings while writers are excluded
    xSemaphoreGive(_mutex);
    return copy;
}

// RAII lock around a writer's mutations to _s.
namespace {
struct Guard {
    SemaphoreHandle_t m;
    explicit Guard(SemaphoreHandle_t mtx) : m(mtx) { if (m) xSemaphoreTake(m, portMAX_DELAY); }
    ~Guard() { if (m) xSemaphoreGive(m); }
};
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
    _s.retryCount = _prefs.getUChar("rcount", 3);
    _s.retryDelaySec = _prefs.getUShort("rdelay", 10);
    _s.theme = _prefs.getString("theme", "dark");
    _s.brightness = _prefs.getUChar("bright", 100);
    _s.sleepTimeoutMin = _prefs.getUShort("sleep", 0);
    _s.tempFahrenheit = _prefs.getBool("tempF", false);
    _s.usAqi = _prefs.getBool("usAqi", true);
    _s.disableSplash = _prefs.getBool("noSplash", false);
    _s.debug = _prefs.getBool("debug", false);
    _s.deviceName = _prefs.getString("devname", "");
    _s.layoutJson = _prefs.getString("layout", defaultLayoutJson());
    LOG_I("settings", "loaded (configured=%s)", isConfigured() ? "yes" : "no");
}

void SettingsManager::setNetwork(const String& ssid1, const String& pass1,
                                 const String& ssid2, const String& pass2) {
    Guard g(_mutex);
    _s.ssid1 = ssid1; _s.pass1 = pass1;
    _s.ssid2 = ssid2; _s.pass2 = pass2;
    _prefs.putString("ssid1", ssid1);
    _prefs.putString("pass1", pass1);
    _prefs.putString("ssid2", ssid2);
    _prefs.putString("pass2", pass2);
    LOG_I("settings", "network saved (ssid1='%s' ssid2='%s')", ssid1.c_str(), ssid2.c_str());
}

void SettingsManager::setApi(const String& endpoint, const String& apiKey,
                             uint16_t pollIntervalSec, uint16_t timeoutSec,
                             uint8_t retryCount, uint16_t retryDelaySec) {
    Guard g(_mutex);
    _s.endpoint = endpoint;
    _s.endpoint.trim();
    _s.apiKey = apiKey;
    _s.pollIntervalSec = max<uint16_t>(pollIntervalSec, 15);
    _s.timeoutSec = constrain(timeoutSec, (uint16_t)3, (uint16_t)60);
    _s.retryCount = constrain(retryCount, (uint8_t)1, (uint8_t)10);
    _s.retryDelaySec = constrain(retryDelaySec, (uint16_t)1, (uint16_t)120);
    _prefs.putString("endpt", _s.endpoint);
    _prefs.putString("apikey", _s.apiKey);
    _prefs.putUShort("poll", _s.pollIntervalSec);
    _prefs.putUShort("timeout", _s.timeoutSec);
    _prefs.putUChar("rcount", _s.retryCount);
    _prefs.putUShort("rdelay", _s.retryDelaySec);
    LOG_I("settings", "api saved (endpoint='%s', retries=%u @ %us)",
          _s.endpoint.c_str(), _s.retryCount, _s.retryDelaySec);
}

void SettingsManager::setGeneral(const String& theme, uint8_t brightness,
                                 uint16_t sleepTimeoutMin, bool tempF, bool usAqi,
                                 bool disableSplash, bool debug) {
    Guard g(_mutex);
    _s.theme = theme;
    _s.brightness = constrain(brightness, (uint8_t)20, (uint8_t)100);
    _s.sleepTimeoutMin = sleepTimeoutMin;
    _s.tempFahrenheit = tempF;
    _s.usAqi = usAqi;
    _s.disableSplash = disableSplash;
    _s.debug = debug;
    _prefs.putString("theme", theme);
    _prefs.putUChar("bright", _s.brightness);
    _prefs.putUShort("sleep", sleepTimeoutMin);
    _prefs.putBool("tempF", tempF);
    _prefs.putBool("usAqi", usAqi);
    _prefs.putBool("noSplash", disableSplash);
    _prefs.putBool("debug", debug);
}

void SettingsManager::setLayoutJson(const String& layoutJson) {
    Guard g(_mutex);
    _s.layoutJson = layoutJson;
    _prefs.putString("layout", layoutJson);
}

void SettingsManager::setDeviceName(const String& name) {
    Guard g(_mutex);
    _s.deviceName = name;
    _s.deviceName.trim();
    _prefs.putString("devname", _s.deviceName);
    LOG_I("settings", "device name override='%s'", _s.deviceName.c_str());
}

void SettingsManager::factoryReset() {
    Guard g(_mutex);
    _prefs.clear();
    load();
    LOG_W("settings", "factory reset complete");
}
