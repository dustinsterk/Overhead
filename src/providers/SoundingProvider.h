#pragma once
#include <Arduino.h>
#include <vector>
#include "TleProvider.h"   // ProviderStatus

class Settings;
class NetClient;
class Cache;
class EventBus;
class LocationService;

// providers/SoundingProvider — atmospheric sounding for the Aviation tab's Skew-T
// / soaring view (spec §14, aviation phase 2). Builds a model sounding at the
// observer from Open-Meteo pressure-level fields (temperature/dewpoint/geopotential
// height/wind per hPa level; no key, works anywhere). Fetches only the current hour
// to stay RAM-lean, parses temp/dewpoint/wind by altitude, derives the freezing
// level. (Was rucsoundings.noaa.gov GSD until NOAA decommissioned that host.)
struct SoundingLevel {
  float altM = 0;
  float tempC = -999;
  float dewpC = -999;
  int   wdir = -1;
  int   wspd = -1;
};

class SoundingProvider {
public:
  void begin(Settings* s, NetClient* net, Cache* cache, EventBus* bus, LocationService* loc);
  void refresh(bool force = false);

  const std::vector<SoundingLevel>& levels() const { return _levels; }
  float freezingLevelM() const { return _freezeM; }   // -1 if none/below ground
  ProviderStatus status() const { return _status; }
  uint32_t lastFetched() const { return _lastFetched; }

  // Derived soaring/aviation analysis (model estimates; -1 = n/a). MSL feet.
  float cloudBaseFt() const { return _cloudBaseFt; }  // LCL from surface T-Td spread
  float thermalTopFt() const { return _thermalTopFt; }// dry-parcel/env crossing
  float inversionFt() const { return _inversionFt; }  // base of first inversion (cap)
  int   stability() const { return _stability; }      // 0 stable 1 neutral 2 unstable 3 strong; -1 n/a

private:
  bool parse(const String& gsd);
  void analyze();          // derive cloud base / stability / lift top / inversion

  Settings*        _s = nullptr;
  NetClient*       _net = nullptr;
  Cache*           _cache = nullptr;
  EventBus*        _bus = nullptr;
  LocationService* _loc = nullptr;

  std::vector<SoundingLevel> _levels;
  float _freezeM = -1;
  float _cloudBaseFt = -1, _thermalTopFt = -1, _inversionFt = -1;
  int   _stability = -1;
  ProviderStatus _status = ProviderStatus::Loading;
  uint32_t _lastFetched = 0;
  bool _inflight = false;
};
