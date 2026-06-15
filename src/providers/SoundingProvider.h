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
// / soaring view (spec §14, aviation phase 2). Fetches an Op40 (RAP) model
// sounding at the observer from rucsoundings.noaa.gov in GSD text format (no key,
// works anywhere — no RAOB station needed). Parses temp/dewpoint/wind by altitude
// and derives the freezing level.
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

private:
  bool parse(const String& gsd);

  Settings*        _s = nullptr;
  NetClient*       _net = nullptr;
  Cache*           _cache = nullptr;
  EventBus*        _bus = nullptr;
  LocationService* _loc = nullptr;

  std::vector<SoundingLevel> _levels;
  float _freezeM = -1;
  ProviderStatus _status = ProviderStatus::Loading;
  uint32_t _lastFetched = 0;
  bool _inflight = false;
};
