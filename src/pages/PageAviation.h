#pragma once
#include "../core/Page.h"
#include <Arduino.h>

class AviationWxProvider;
class LocationService;

// pages/PageAviation — nearby aviation weather (spec §14). Decoded METAR card for
// the selected/nearest station (wind, vis, ceiling, temp/dewpoint, altimeter,
// flight category) + raw METAR + raw TAF. Tap edges to step through stations.
// Phase 1 (METAR/TAF); Skew-T soundings are a follow-up (BACKLOG).
class PageAviation : public Page {
public:
  PageAviation(AviationWxProvider& wx, LocationService& loc) : _wx(wx), _loc(loc) {}

  const char* title() const override { return "Aviation"; }
  void onEnter(App& app) override { _dirty = _needClear = true; }
  void onData(App& app, ProviderId id) override;
  void onTouch(App& app, int x, int y) override;
  void tick(App& app, uint32_t nowMs) override;

private:
  void draw(App& app);

  AviationWxProvider& _wx;
  LocationService&    _loc;
  int   _sel = 0;
  bool  _dirty = true;
  bool  _needClear = true;
  bool  _wasEmpty = true;
  uint32_t _lastDraw = 0;
};
