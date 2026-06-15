#pragma once
#include "../core/Page.h"
#include <Arduino.h>

class TimeService;
class LocationService;

// pages/PageDiag — the milestone-1 proof page. Shows board, WiFi, NTP time,
// active location, heap and the OTA/web address, and redraws when Time/Location
// updates arrive over the EventBus — demonstrating the full
// background-fetch -> cache -> EventBus -> page-redraw path. Tap to force a
// location re-resolve. (The real System/Health overlay is milestone 11.)
class PageDiag : public Page {
public:
  PageDiag(TimeService& time, LocationService& loc, String hostname)
    : _time(time), _loc(loc), _host(std::move(hostname)) {}

  const char* title() const override { return "Diagnostics"; }
  void onEnter(App& app) override { _dirty = true; }
  void onData(App& app, ProviderId id) override { _dirty = true; }
  void onTouch(App& app, int x, int y) override;
  void tick(App& app, uint32_t nowMs) override;

private:
  void draw(App& app);

  TimeService&     _time;
  LocationService& _loc;
  String           _host;
  bool             _dirty       = true;
  uint32_t         _lastDrawMs  = 0;
};
