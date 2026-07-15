#include "AirGradientClient.h"
#include <memory>
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
    // 24KB stack: HTTP + JSON parsing headroom. Runs on core 0, away from the
    // LVGL/loop core, so a poll never stalls the UI. (Shared state with the
    // UI thread is mutex-protected; see snapshot() and latest().)
    xTaskCreatePinnedToCore(taskEntry, "ag_api", 24576, this, 1, &_task, 0);
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
            // Safety net: never let a leak/fragmentation on the failure path
            // run the heap to zero and panic — skip the poll and back off.
            if (ESP.getFreeHeap() < 45000) {
                LOG_W("api", "low heap (%u B) - skipping poll", ESP.getFreeHeap());
                setLastErr("low heap - skipped");
                _lastError = ApiError::BadResponse;
                break;
            }
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

String AirGradientClient::lastErrorText() const {
    if (!_mutex) return _lastHttpErr;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    String t = _lastHttpErr;
    xSemaphoreGive(_mutex);
    return t;
}

void AirGradientClient::setLastErr(const String& text) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _lastHttpErr = text;
    xSemaphoreGive(_mutex);
}

String AirGradientClient::lastUrl() const {
    if (!_mutex) return _lastUrl;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    String u = _lastUrl;
    xSemaphoreGive(_mutex);
    return u;
}

void AirGradientClient::setUrl(const String& url) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _lastUrl = url;
    xSemaphoreGive(_mutex);
}

bool AirGradientClient::fetchOnce(const AppSettings& s) {
    // The endpoint setting is a base URL or IP (local AirGradient device,
    // e.g. "http://192.168.1.50" or "http://airgradient_xxxx.local"); the
    // sensor's local server exposes GET /measures/current. A full cloud URL
    // that already contains the path is used as-is.
    String url = s.endpoint;
    url.trim();
    // Default to plain HTTP (local devices). Only the official cloud API is
    // HTTPS unless the user typed the scheme explicitly.
    bool cloud = url.indexOf("api.airgradient.com") >= 0;
    if (!url.startsWith("http")) {
        url = (cloud ? "https://" : "http://") + url;
    }
    while (url.endsWith("/")) url.remove(url.length() - 1);
    if (url.indexOf("/measures/current") < 0) url += "/measures/current";
    // Token is ONLY for the cloud API. Local endpoints never get one — this
    // ignores any stale token left in NVS from an earlier cloud config.
    if (cloud && s.apiKey.length() > 0 && url.indexOf("token=") < 0) {
        url += (url.indexOf('?') >= 0) ? "&" : "?";
        url += "token=" + s.apiKey;
    }
    setUrl(url);  // expose the exact URL for the debug overlay

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
    if (!isLiteralIp) {
        // A DNS name the ESP itself can't resolve is a common cause of the
        // "timeout" you see when the host pings fine from your PC.
        resolved = (WiFi.hostByName(host.c_str(), ip) == 1) ? ip.toString()
                                                            : String("(dns failed)");
    }
    setHostInfo(host, resolved);

    bool https = url.startsWith("https");

    // ORDER MATTERS: the transport client must be declared BEFORE HTTPClient.
    // HTTPClient stores a raw pointer to it, and locals are destroyed in
    // reverse declaration order — with the client declared second it died
    // first, so ~HTTPClient() then used a dangling pointer and tore down a
    // socket lwip still owned. The next inbound ACK hit a freed pbuf and
    // aborted the tcpip thread ("pbuf_free: p->ref > 0"). Client first =
    // destroyed last = HTTPClient always unwinds against a live client.
    //
    // The secure client is only constructed for real HTTPS URLs: building an
    // mbedTLS context per poll for a plain-HTTP local endpoint leaks heap.
    WiFiClient plainClient;
    std::unique_ptr<WiFiClientSecure> secureClient;
    if (https) {
        secureClient.reset(new WiFiClientSecure());
        secureClient->setInsecure();  // no cert pinning
        secureClient->setHandshakeTimeout(s.timeoutSec);  // seconds
    }

    HTTPClient http;
    http.setReuse(false);  // fresh connection each poll; avoids keep-alive bugs
    http.setTimeout(s.timeoutSec * 1000);
    http.setConnectTimeout(s.timeoutSec * 1000);

    bool ok = https ? http.begin(*secureClient, url) : http.begin(plainClient, url);
    if (!ok) {
        LOG_E("api", "http.begin failed");
        setLastErr("begin failed");
        _lastError = ApiError::BadResponse;
        return false;
    }

    int code = http.GET();
    if (code <= 0) {
        String reason = http.errorToString(code);
        LOG_W("api", "request failed: %s", reason.c_str());
        setLastErr(reason);
        _lastError = ApiError::Timeout;
        http.end();
        return false;
    }
    if (code == 401 || code == 403) {
        LOG_W("api", "auth rejected (HTTP %d) - check token", code);
        setLastErr("HTTP " + String(code) + " auth");
        _lastError = ApiError::AuthFailed;
        http.end();
        return false;
    }
    if (code != 200) {
        LOG_W("api", "unexpected HTTP %d", code);
        setLastErr("HTTP " + String(code));
        _lastError = ApiError::BadResponse;
        http.end();
        return false;
    }

    AirGradientReading fresh;
    bool parsed = parsePayload(http.getStream(), fresh);
    http.end();
    if (parsed) setLastErr("");  // clear on success

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
    // No filter: the local server payload is a small flat object, and the
    // cloud API is a small array of locations. (A JsonDocument filter can't
    // be both an array and an object, so filtering silently dropped the
    // whole local-server response.) Both fit easily unfiltered.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, stream);
    if (err) {
        LOG_W("api", "json parse error: %s", err.c_str());
        setLastErr(String("json: ") + err.c_str());
        return false;
    }

    // Local server -> object; cloud /locations -> array of locations.
    JsonObjectConst obj;
    if (doc.is<JsonArrayConst>()) {
        JsonArrayConst arr = doc.as<JsonArrayConst>();
        if (arr.size() == 0) {
            LOG_W("api", "empty location list");
            setLastErr("empty location list");
            return false;
        }
        obj = arr[0];
    } else {
        obj = doc.as<JsonObjectConst>();
    }
    if (obj.isNull()) {
        setLastErr("json: not an object");
        return false;
    }

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
    if (!out.valid) setLastErr("json ok but no sensor fields");
    return out.valid;
}
