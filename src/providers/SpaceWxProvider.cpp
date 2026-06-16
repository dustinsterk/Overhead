#include "SpaceWxProvider.h"
#include "../services/Settings.h"
#include "../services/NetClient.h"
#include "../services/Cache.h"
#include "../core/EventBus.h"
#include <ArduinoJson.h>
#include <time.h>

void SpaceWxProvider::begin(Settings* s, NetClient* net, Cache* cache, EventBus* bus) {
  _s = s; _net = net; _cache = cache; _bus = bus;
  String body; CacheMeta m;
  if (_cache->get("swx_kp", body, m) && parseKp(body)) _status = ProviderStatus::Stale;
  if (_cache->get("swx_sfi", body, m)) { JsonDocument d; if (!deserializeJson(d, body)) _sfi = (int)((float)atof((const char*)(d["Flux"] | "-1"))); }
  refresh(false);
}

void SpaceWxProvider::refresh(bool force) {
  uint32_t ttl = (uint32_t)_s->getInt("refreshSpaceWxMin", 20) * 60UL;
  uint32_t now = (uint32_t)time(nullptr);
  CacheMeta m = _cache->stat("swx_kp");
  bool stale = force || !m.found || now < 1600000000UL || (now - m.fetchedAt) > ttl;
  if (stale) { fetchKp(); fetchSfi(); }
}

void SpaceWxProvider::fetchKp() {
  _net->get("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json",
    [this](int code, const String& body) {
      if (code == 200 && parseKp(body)) {
        _cache->put("swx_kp", body, code, (uint32_t)time(nullptr));
        _lastFetched = (uint32_t)time(nullptr);
        _status = ProviderStatus::Ready;
        Serial.printf("[spacewx] Kp=%.1f\n", _kp);
      } else {
        Serial.printf("[spacewx] Kp fetch failed code=%d len=%u\n", code, (unsigned)body.length());
        if (_kp < 0) _status = ProviderStatus::Error;
      }
      if (_bus) _bus->publish(ProviderId::SpaceWx);
    });
}

void SpaceWxProvider::fetchSfi() {
  _net->get("https://services.swpc.noaa.gov/products/summary/10cm-flux.json",
    [this](int code, const String& body) {
      if (code == 200) {
        JsonDocument d;
        if (!deserializeJson(d, body)) {
          JsonVariant fv = d["Flux"];
          if (fv.isNull()) fv = d["flux"];
          _sfi = fv.is<const char*>() ? atoi((const char*)fv) : (fv.isNull() ? -1 : fv.as<int>());
          _cache->put("swx_sfi", body, code, (uint32_t)time(nullptr));
          if (_sfi < 0) Serial.printf("[spacewx] SFI not found, body: %.90s\n", body.c_str());
          else          Serial.printf("[spacewx] SFI=%d\n", _sfi);
        }
      } else {
        Serial.printf("[spacewx] SFI fetch code=%d\n", code);
      }
      if (_bus) _bus->publish(ProviderId::SpaceWx);
    });
}

// Kp feed is an array of rows; row[0] is a header, the last row is most recent:
// [ "time_tag", "Kp", "a_running", "station_count" ].
bool SpaceWxProvider::parseKp(const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { Serial.printf("[spacewx] kp parse err: %s\n", err.c_str()); return false; }
  JsonArray rows = doc.as<JsonArray>();
  if (rows.isNull() || rows.size() < 2) { Serial.println("[spacewx] kp: not an array"); return false; }
  // Scan backward for the most recent row with a real Kp (the latest slots are
  // often blank, and the value can be a string or a number).
  for (int i = (int)rows.size() - 1; i >= 1; --i) {
    JsonArray r = rows[i].as<JsonArray>();
    if (r.isNull() || r.size() < 2) continue;
    JsonVariant kv = r[1];
    if (kv.is<const char*>()) { const char* s = (const char*)kv; if (!s || !s[0]) continue; _kp = atof(s); }
    else if (kv.is<float>() || kv.is<int>()) _kp = kv.as<float>();
    else continue;
    Serial.printf("[spacewx] Kp=%.2f (row %d/%u)\n", _kp, i, (unsigned)rows.size());
    return _kp >= 0;
  }
  Serial.println("[spacewx] kp: no numeric row");
  return false;
}
