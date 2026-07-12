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
    // 12KB stack: TLS handshake + JSON parsing headroom. Runs on core 0,
    // away from the LVGL/loop core.
    xTaskCreatePinnedToCore(taskEntry, "ag_api", 12288, this, 1, &_task, 0);
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

        if (!_wifi->isConnected()) {
            _lastError = ApiError::NoNetwork;
            continue;
        }
        if (_settings->get().apiKey.length() == 0) {
            _lastError = ApiError::AuthFailed;
            continue;
        }

        _lastAttemptMs = millis();
        if (fetchOnce()) {
            _failures = 0;
            _lastError = ApiError::None;
        } else {
            _failures = _failures + 1;
            // Quick retry once after 5s on failure, then fall back to cadence.
            if (_failures == 1) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                if (_wifi->isConnected() && fetchOnce()) {
                    _failures = 0;
                    _lastError = ApiError::None;
                }
            }
        }
    }
}

bool AirGradientClient::fetchOnce() {
    const AppSettings& s = _settings->get();

    String url = s.endpoint;
    if (url.indexOf("token=") < 0) {
        url += (url.indexOf('?') >= 0) ? "&" : "?";
        url += "token=" + s.apiKey;
    }

    HTTPClient http;
    http.setTimeout(s.timeoutSec * 1000);
    http.setConnectTimeout(s.timeoutSec * 1000);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    bool https = url.startsWith("https");
    bool ok;
    if (https) {
        secureClient.setInsecure();  // MVP: no cert pinning
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
        LOG_W("api", "auth rejected (HTTP %d) — check API key", code);
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
    filterEl["pm02_corrected"] = true;
    filterEl["rco2"] = true;
    filterEl["rco2_corrected"] = true;
    filterEl["atmp"] = true;
    filterEl["atmp_corrected"] = true;
    filterEl["rhum"] = true;
    filterEl["rhum_corrected"] = true;
    filterEl["tvocIndex"] = true;
    filterEl["noxIndex"] = true;
    filterEl["wifi"] = true;
    filterEl["timestamp"] = true;

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

    auto num = [&](const char* corrected, const char* raw) -> float {
        if (obj[corrected].is<float>()) return obj[corrected].as<float>();
        if (obj[raw].is<float>()) return obj[raw].as<float>();
        return NAN;
    };

    out.pm25 = num("pm02_corrected", "pm02");
    out.co2 = num("rco2_corrected", "rco2");
    out.temperature = num("atmp_corrected", "atmp");
    out.humidity = num("rhum_corrected", "rhum");
    out.tvoc = obj["tvocIndex"] | NAN;
    out.nox = obj["noxIndex"] | NAN;
    out.aqi = aqiFromPm25(out.pm25);
    out.wifiRssi = obj["wifi"] | 0;
    out.deviceName = obj["locationName"] | "";
    out.timestamp = obj["timestamp"] | "";
    out.receivedAtMs = millis();
    out.valid = !isnan(out.pm25) || !isnan(out.co2) || !isnan(out.temperature);
    return out.valid;
}
