#include "SoundingProvider.h"
#include "../services/Settings.h"
#include "../services/NetClient.h"
#include "../services/Cache.h"
#include "../services/LocationService.h"
#include "../core/EventBus.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <math.h>
#include <time.h>

// Pressure levels (hPa) sampled for the profile: surface (~1000) up to ~13 km.
static const int kLevels[] = { 1000, 925, 850, 700, 600, 500, 400, 300, 250, 200, 150 };
static constexpr int kNL = sizeof(kLevels) / sizeof(kLevels[0]);

void SoundingProvider::begin(Settings* s, NetClient* net, Cache* cache, EventBus* bus, LocationService* loc) {
  _s = s; _net = net; _cache = cache; _bus = bus; _loc = loc;
  String body; CacheMeta m;
  if (_cache->get("sounding", body, m)) parse(body);
  refresh(false);
}

void SoundingProvider::refresh(bool force) {
  if (_inflight || !_loc->active().valid) return;
  uint32_t ttl = 60UL * 60UL;                       // hourly model run
  uint32_t now = (uint32_t)time(nullptr);
  CacheMeta m = _cache->stat("sounding");
  bool stale = force || !m.found || now < 1600000000UL || (now - m.fetchedAt) > ttl;
  if (!stale) return;

  // Open-Meteo pressure-level fields (rucsoundings.noaa.gov was decommissioned).
  // Fetch only the current hour (start_hour == end_hour -> a single sample) so the
  // JSON stays small enough for the no-PSRAM boards. wind in knots, height in m.
  time_t t = time(nullptr); struct tm g; gmtime_r(&t, &g);
  char hr[20]; strftime(hr, sizeof(hr), "%Y-%m-%dT%H:00", &g);

  String hourly;
  char f[48];
  for (int i = 0; i < kNL; ++i) {
    int L = kLevels[i];
    if (i) hourly += ',';
    snprintf(f, sizeof(f), "temperature_%dhPa,dewpoint_%dhPa,", L, L);                    hourly += f;
    snprintf(f, sizeof(f), "geopotential_height_%dhPa,", L);                              hourly += f;
    snprintf(f, sizeof(f), "wind_speed_%dhPa,wind_direction_%dhPa", L, L);                hourly += f;
  }
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(_loc->active().lat, 3)
             + "&longitude=" + String(_loc->active().lon, 3)
             + "&wind_speed_unit=kn&start_hour=" + hr + "&end_hour=" + hr
             + "&hourly=" + hourly;

  _inflight = true;
  bool sent = _net->get(url, [this](int code, const String& body) {
    _inflight = false;
    if (code == 200 && parse(body)) {
      _cache->put("sounding", body, code, (uint32_t)time(nullptr));
      _lastFetched = (uint32_t)time(nullptr);
      _status = ProviderStatus::Ready;
      Serial.printf("[sounding] %u levels, FZL=%.0fm\n", (unsigned)_levels.size(), _freezeM);
    } else {
      Serial.printf("[sounding] fetch failed code=%d len=%u\n", code, (unsigned)body.length());
      if (_levels.empty()) _status = ProviderStatus::Error;
    }
    if (_bus) _bus->publish(ProviderId::Weather);
  });
  if (!sent) _inflight = false;
}

// Open-Meteo "hourly" object: each pressure-level field is an array; we kept a
// single hour, so element [0] is the value. Build the profile, sort by altitude,
// derive the freezing level (first upward T=0 crossing).
bool SoundingProvider::parse(const String& body) {
  JsonDocument filter;
  JsonObject hf = filter["hourly"].to<JsonObject>();
  char key[40];
  for (int i = 0; i < kNL; ++i) {
    int L = kLevels[i];
    snprintf(key, sizeof(key), "temperature_%dhPa", L);          hf[key] = true;
    snprintf(key, sizeof(key), "dewpoint_%dhPa", L);             hf[key] = true;
    snprintf(key, sizeof(key), "geopotential_height_%dhPa", L);  hf[key] = true;
    snprintf(key, sizeof(key), "wind_speed_%dhPa", L);           hf[key] = true;
    snprintf(key, sizeof(key), "wind_direction_%dhPa", L);       hf[key] = true;
  }

  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  JsonObject h = doc["hourly"];
  if (h.isNull()) return false;

  std::vector<SoundingLevel> out;
  for (int i = 0; i < kNL; ++i) {
    int L = kLevels[i];
    snprintf(key, sizeof(key), "temperature_%dhPa", L);
    JsonVariant tv = h[key][0];
    if (tv.isNull()) continue;
    snprintf(key, sizeof(key), "geopotential_height_%dhPa", L);
    JsonVariant gv = h[key][0];
    if (gv.isNull()) continue;
    SoundingLevel lv;
    lv.tempC = tv.as<float>();
    lv.altM  = gv.as<float>();
    snprintf(key, sizeof(key), "dewpoint_%dhPa", L);
    JsonVariant dv = h[key][0];   lv.dewpC = dv.isNull() ? -999 : dv.as<float>();
    snprintf(key, sizeof(key), "wind_speed_%dhPa", L);
    JsonVariant wv = h[key][0];   lv.wspd  = wv.isNull() ? -1 : (int)lround(wv.as<float>());
    snprintf(key, sizeof(key), "wind_direction_%dhPa", L);
    JsonVariant wd = h[key][0];   lv.wdir  = wd.isNull() ? -1 : (int)lround(wd.as<float>());
    out.push_back(lv);
  }
  if (out.size() < 3) return false;
  std::sort(out.begin(), out.end(), [](const SoundingLevel& a, const SoundingLevel& b) { return a.altM < b.altM; });

  float fz = -1;
  for (size_t k = 1; k < out.size(); ++k) {
    if (out[k - 1].tempC > 0 && out[k].tempC <= 0) {
      float f2 = out[k - 1].tempC / (out[k - 1].tempC - out[k].tempC);
      fz = out[k - 1].altM + f2 * (out[k].altM - out[k - 1].altM);
      break;
    }
  }
  _levels = std::move(out);
  _freezeM = fz;
  analyze();
  return true;
}

// Derived soaring/aviation analysis from the profile (low-precision estimates).
void SoundingProvider::analyze() {
  _cloudBaseFt = _thermalTopFt = _inversionFt = -1; _stability = -1;
  if (_levels.size() < 2) return;
  const float M2FT = 3.28084f, DALR = 9.8f;          // dry adiabatic lapse, C/km
  const SoundingLevel& s = _levels.front();
  float t0 = s.tempC, td0 = s.dewpC, a0 = s.altM;

  // Cloud base (LCL): ~125 m of lift per C of surface temp-dewpoint spread.
  if (td0 > -900) _cloudBaseFt = (a0 + 125.0f * max(0.0f, t0 - td0)) * M2FT;

  // Low-level lapse rate (surface -> ~1 km up) -> stability class.
  const SoundingLevel* hi = nullptr;
  for (const auto& l : _levels) if (l.altM >= a0 + 1000.0f) { hi = &l; break; }
  if (!hi) hi = &_levels.back();
  float dz = hi->altM - a0;
  if (dz > 50.0f) {
    float elr = (t0 - hi->tempC) / (dz / 1000.0f);   // C/km
    _stability = elr >= 8.5f ? 3 : elr >= 6.5f ? 2 : elr >= 4.0f ? 1 : 0;
  }

  // Top of (dry) lift: lift a surface parcel up the dry adiabat; the height where
  // it stops being warmer than the environment. Plot axes are linear, so this is
  // where the (straight) parcel line crosses the temperature curve.
  float prevDiff = 0;                                // parcel == env at the surface
  _thermalTopFt = a0 * M2FT;
  for (size_t i = 1; i < _levels.size(); ++i) {
    float pT = t0 - DALR * (_levels[i].altM - a0) / 1000.0f;
    float diff = pT - _levels[i].tempC;
    if (diff < 0) {
      float frac = prevDiff / (prevDiff - diff);     // 0..1 between i-1 and i
      _thermalTopFt = (_levels[i - 1].altM + frac * (_levels[i].altM - _levels[i - 1].altM)) * M2FT;
      break;
    }
    prevDiff = diff;
  }

  // First inversion base (temperature rising with height) = the lift cap.
  for (size_t i = 1; i < _levels.size(); ++i)
    if (_levels[i].tempC > _levels[i - 1].tempC + 0.1f) { _inversionFt = _levels[i - 1].altM * M2FT; break; }
}
