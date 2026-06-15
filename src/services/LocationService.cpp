#include "LocationService.h"
#include "Settings.h"
#include "NetClient.h"
#include "TimeService.h"
#include "../core/EventBus.h"
#include <ArduinoJson.h>

bool LocationService::begin(Settings* s, NetClient* net, EventBus* bus, TimeService* time) {
  _s = s; _net = net; _bus = bus; _time = time;
  refresh();
  return true;
}

void LocationService::refresh() {
  String mode = _s->getString("locMode", "auto");
  if (mode == "preset") {
    GeoLocation loc;
    loc.lat = _s->getFloat("locLat", 0);
    loc.lon = _s->getFloat("locLon", 0);
    loc.tzOffsetSec = _s->getInt("tzOffset", 0);
    loc.name = _s->getString("locName", "Preset");
    loc.valid = true;
    applyAndPublish(loc, /*persist=*/false);
  } else {
    refreshAuto();                       // "auto" (and "gps" until a module exists)
  }
}

void LocationService::refreshAuto() {
  // ip-api.com: keyless, returns lat/lon/city + tz offset in seconds.
  const char* url = "http://ip-api.com/json/?fields=status,message,lat,lon,city,regionName,offset";
  _net->get(url, [this](int code, const String& body) {
    if (code != 200) { Serial.printf("[loc] IP geoloc failed (code %d)\n", code); return; }
    JsonDocument doc;
    if (deserializeJson(doc, body)) { Serial.println("[loc] IP geoloc parse error"); return; }
    if (String((const char*)(doc["status"] | "")) != "success") {
      Serial.printf("[loc] IP geoloc: %s\n", (const char*)(doc["message"] | "?"));
      return;
    }
    GeoLocation loc;
    loc.lat = doc["lat"] | 0.0;
    loc.lon = doc["lon"] | 0.0;
    loc.tzOffsetSec = doc["offset"] | 0;
    String city = String((const char*)(doc["city"] | ""));
    loc.name = city.length() ? city : String("Auto (IP)");
    loc.valid = true;
    applyAndPublish(loc, /*persist=*/true);
  });
}

void LocationService::applyAndPublish(const GeoLocation& loc, bool persist) {
  _loc = loc;
  Serial.printf("[loc] %s  %.4f, %.4f  tz%+ld\n",
                loc.name.c_str(), loc.lat, loc.lon, loc.tzOffsetSec);
  if (_time) _time->applyTzOffset(loc.tzOffsetSec);
  if (persist && _s) {
    _s->set("locLat", loc.lat);
    _s->set("locLon", loc.lon);
    _s->set("locName", loc.name.c_str());
    _s->set("tzOffset", (long)loc.tzOffsetSec);
    _s->save();
  }
  if (_bus) _bus->publish(ProviderId::Location);
}
