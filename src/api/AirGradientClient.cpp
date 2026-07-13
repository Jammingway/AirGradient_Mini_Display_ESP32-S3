#include "AirGradientClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../settings/SettingsManager.h"
#include "../network/WifiManager.h"
#include "../utils/Logger.h"

void AirGradientClient::begin(SettingsManager& settings, WifiManager& wifi) {
    _settings = &settings;
    _wifi = &wifi;
    _mutex = xSemaphoreCreateMutex();
    // 16KB stack: TLS handshake + JSON parsing headroom. Runs on core 0,
    // away from the LVGL/loop core.
    xTaskCreatePinnedToCore(taskEntry, "ag_api", 16384, this, 1, &_task, 0);
}

bool AirGradientClient::latest(AirGradientReading& out) const {
    if (!_mutex) return false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    out = _reading;
    xSemaphoreGive(_mutex);
    return out.valid;
}

void AirGradientClient::requestNow() {
    if (_task) xTaskNotifyGive(_task);
}

void AirGradientClient::taskEntry(void* arg) {
    static_cast<AirGradientClient*>(arg)->taskLoop();
}

void AirGradientClient::taskLoop() {
    for (;;) {
        uint32_t intervalMs = (uint32_t)_settings->get().pollIntervalSec * 1000;
        // Sleep until the next poll is due, or a manual refresh pokes us.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(intervalMs));

        // Snapshot settings once per cycle (thread-safe copy). Never read
        // the live settings from this task — the UI thread rewrites them.
        AppSettings s = _settings->snapshot();

        if (!_wifi->isConnected()) {
            _lastError = ApiError::NoNetwork;
            continue;
        }
        // Token is optional (local endpoints); only the endpoint is required.
        if (s.endpoint.length() == 0) {
            _lastError = ApiError::BadResponse;
            continue;
        }

        // Each poll cycle makes up to retryCount attempts, retryDelaySec
        // apart (user-configurable in Settings -> Endpoint).
        uint8_t attempts = max<uint8_t>(1, s.retryCount);
        uint16_t delaySec = max<uint16_t>(1, s.retryDelaySec);
        for (uint8_t attempt = 1; attempt <= attempts; attempt++) {
            _lastAttemptMs = millis();
            if (fetchOnce(s)) {
                _failures = 0;
                _lastError = ApiError::None;
                break;
            }
            _failures = _failures + 1;
            if (attempt < attempts) {
                LOG_W("api", "attempt %u/%u failed - retrying in %u s",
                      attempt, attempts, delaySec);
                vTaskDelay(pdMS_TO_TICKS((uint32_t)delaySec * 1000));
                if (!_wifi->isConnected()) {
                    _lastError = ApiError::NoNetwork;
                    break;
                }
            } else {
                LOG_W("api", "all %u attempts failed this cycle", attempts);
            }
        }
    }
}

String AirGradientClient::host() const {
    if (!_mutex) return _host;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    String h = _host;
    xSemaphoreGive(_mutex);
    return h;
}

String AirGradientClient::resolvedIp() const {
    if (!_mutex) return _resolvedIp;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    String ip = _resolvedIp;
    xSemaphoreGive(_mutex);
    return ip;
}

void AirGradientClient::setHostInfo(const String& host, const String& ip) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _host = host;
    _resolvedIp = ip;
    xSemaphoreGive(_mutex);
}

bool AirGradientClient::fetchOnce(const AppSettings& s) {
    // The endpoint setting is a base URL or IP (local AirGradient device,
    // e.g. "http://192.168.1.50" or "http://airgradient_xxxx.local"); the
    // sensor's local server exposes GET /measures/current. A full cloud URL
    // that already contains the path is used as-is.
    String url = s.endpoint;
    if (!url.startsWith("http")) url = "http://" + url;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    if (url.indexOf("/measures/current") < 0) url += "/measures/current";
    // Token is only needed by the cloud API; local endpoints skip it.
    if (s.apiKey.length() > 0 && url.indexOf("token=") < 0) {
        url += (url.indexOf('?') >= 0) ? "&" : "?";
        url += "token=" + s.apiKey;
    }

    // Extract the bare host (no scheme, no port, no path) for the status
    // screen, and resolve it if it is a DNS name.
    String host = url;
    int schemeEnd = host.indexOf("://");
    if (schemeEnd >= 0) host = host.substring(schemeEnd + 3);
    int slash = host.indexOf('/');
    if (slash >= 0) host = host.substring(0, slash);
    int colon = host.indexOf(':');
    if (colon >= 0) host = host.substring(0, colon);
    String resolved;
    IPAddress ip;
    bool isLiteralIp = ip.fromString(host);
    if (!isLiteralIp && WiFi.hostByName(host.c_str(), ip) == 1) {
        resolved = ip.toString();
    }
    setHostInfo(host, resolved);

    HTTPClient http;
    http.setTimeout(s.timeoutSec * 1000);
    http.setConnectTimeout(s.timeoutSec * 1000);

    bool https = url.startsWith("https");
    bool ok;
    // Only construct the (heavier) TLS client when actually needed; local
    // endpoints are plain HTTP, and building a WiFiClientSecure per poll
    // wastes stack/heap.
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    if (https) {
        secureClient.setInsecure();  // no cert pinning
        secureClient.setHandshakeTimeout(s.timeoutSec);  // seconds
        ok = http.begin(secureClient, url);
    } else {
        ok = http.begin(plainClient, url);
    }
    if (!ok) {
        LOG_E("api", "http.begin failed");
        _lastError = ApiError::BadResponse;
        return false;
    }

    int code = http.GET();
    if (code <= 0) {
        LOG_W("api", "request failed: %s", http.errorToString(code).c_str());
        _lastError = ApiError::Timeout;
        http.end();
        return false;
    }
    if (code == 401 || code == 403) {
        LOG_W("api", "auth rejected (HTTP %d) - check token", code);
        _lastError = ApiError::AuthFailed;
        http.end();
        return false;
    }
    if (code != 200) {
        LOG_W("api", "unexpected HTTP %d", code);
        _lastError = ApiError::BadResponse;
        http.end();
        return false;
    }

    AirGradientReading fresh;
    bool parsed = parsePayload(http.getStream(), fresh);
    http.end();

    if (!parsed) {
        _lastError = ApiError::BadResponse;
        return false;
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    _reading = fresh;
    xSemaphoreGive(_mutex);
    LOG_I("api", "reading ok: pm2.5=%.1f co2=%.0f t=%.1fC rh=%.0f%%",
          fresh.pm25, fresh.co2, fresh.temperature, fresh.humidity);
    return true;
}

bool AirGradientClient::parsePayload(Stream& stream, AirGradientReading& out) {
    // Only pull the fields we need; works for both the /locations array
    // response and a single-location object.
    JsonDocument filterEl;
    filterEl["locationName"] = true;
    filterEl["pm02"] = true;
    filterEl["pm02_corrected"] = true;   // cloud API naming
    filterEl["pm02Compensated"] = true;  // local server naming
    filterEl["rco2"] = true;
    filterEl["rco2_corrected"] = true;
    filterEl["atmp"] = true;
    filterEl["atmp_corrected"] = true;
    filterEl["atmpCompensated"] = true;
    filterEl["rhum"] = true;
    filterEl["rhum_corrected"] = true;
    filterEl["rhumCompensated"] = true;
    filterEl["tvocIndex"] = true;
    filterEl["noxIndex"] = true;
    filterEl["wifi"] = true;
    filterEl["timestamp"] = true;
    filterEl["model"] = true;
    filterEl["serialno"] = true;

    JsonDocument filter;
    filter[0] = filterEl;                    // array responses
    for (JsonPairConst kv : filterEl.as<JsonObjectConst>())  // object responses
        filter[kv.key().c_str()] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
    if (err) {
        LOG_W("api", "json parse error: %s", err.c_str());
        return false;
    }

    JsonObjectConst obj;
    if (doc.is<JsonArrayConst>()) {
        JsonArrayConst arr = doc.as<JsonArrayConst>();
        if (arr.size() == 0) {
            LOG_W("api", "empty location list");
            return false;
        }
        obj = arr[0];
    } else {
        obj = doc.as<JsonObjectConst>();
    }
    if (obj.isNull()) return false;

    // Preference order: local "Compensated" > cloud "_corrected" > raw.
    auto num = [&](const char* compensated, const char* corrected,
                   const char* raw) -> float {
        if (obj[compensated].is<float>()) return obj[compensated].as<float>();
        if (obj[corrected].is<float>()) return obj[corrected].as<float>();
        if (obj[raw].is<float>()) return obj[raw].as<float>();
        return NAN;
    };

    out.pm25 = num("pm02Compensated", "pm02_corrected", "pm02");
    out.co2 = num("rco2_corrected", "rco2", "rco2");  // no Compensated variant
    out.temperature = num("atmpCompensated", "atmp_corrected", "atmp");
    out.humidity = num("rhumCompensated", "rhum_corrected", "rhum");
    out.tvoc = obj["tvocIndex"] | NAN;
    out.nox = obj["noxIndex"] | NAN;
    out.aqi = aqiFromPm25(out.pm25);
    out.wifiRssi = obj["wifi"] | 0;
    // Local payloads identify the sensor by model/serial; cloud by location.
    out.deviceName = obj["locationName"] | "";
    if (out.deviceName.length() == 0) out.deviceName = obj["model"] | "";
    if (out.deviceName.length() == 0) out.deviceName = obj["serialno"] | "";
    out.timestamp = obj["timestamp"] | "";
    out.receivedAtMs = millis();
    out.valid = !isnan(out.pm25) || !isnan(out.co2) || !isnan(out.temperature);
    return out.valid;
}
