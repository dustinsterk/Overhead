#include "PageAgenda.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../services/TimeService.h"
#include "../services/LocationService.h"
#include "../providers/WeatherProvider.h"
#include "../providers/TleProvider.h"
#include "../providers/LaunchProvider.h"
#include "../services/Settings.h"
#include "../astro/Sun.h"
#include "../astro/Time.h"
#include "../astro/SolarSystem.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <time.h>

static Color darknessShade(float sunAlt) {
  if (sunAlt > -0.5) return rgb565(46, 52, 66);     // day
  if (sunAlt > -6)   return rgb565(32, 36, 50);     // civil
  if (sunAlt > -12)  return rgb565(20, 22, 34);     // nautical
  if (sunAlt > -18)  return rgb565(12, 13, 22);     // astronomical
  return rgb565(6, 7, 12);                          // night
}

void PageAgenda::recompute() {
  if (!_time.synced() || !_loc.active().valid) return;
  double lat = _loc.active().lat, lon = _loc.active().lon;
  _base = time(nullptr);

  for (int h = 0; h < kHours; ++h) {
    time_t t = _base + (time_t)h * 3600;
    double jd = astro::julianDate(t);
    _sunAlt[h] = (float)astro::sunAltitudeDeg(jd, lat, lon);
    _cloud[h]  = (int8_t)_wx.cloudCoverAt(t);
    _moonUp[h] = astro::planetState(astro::Planet::Moon, jd, lat, lon).above;
  }

  // Events: next pass per watchlisted bird + upcoming launches (next 24 h).
  _events.clear();
  time_t now = _base, horizon = _base + (time_t)kHours * 3600;
  if (!_tle.sats().empty()) {
    _eng.setObserver(lat, lon, 0);
    int minEl = (int)_settings.getInt("satMinEl", 10);
    JsonArray wl = _settings.doc()["watchlist"].as<JsonArray>();
    for (JsonVariant v : wl) {
      String pre = (const char*)(v | "");
      if (!pre.length()) continue;
      for (const auto& s : _tle.sats()) {
        if (!s.name.startsWith(pre)) continue;
        _eng.loadTle(s.name.c_str(), s.line1.c_str(), s.line2.c_str());
        astro::SatPass p = _eng.nextPass(now, (double)minEl, 40);
        if (p.valid && p.aos < horizon)
          _events.push_back({ p.aos, s.name + " " + (int)round(p.maxElDeg) + (char)247, false });
        break;
      }
    }
  }
  for (const auto& l : _launch.launches())
    if (l.net && l.net > now && l.net < horizon)
      _events.push_back({ l.net, l.name, true });
  std::sort(_events.begin(), _events.end(), [](const Event& a, const Event& b) { return a.t < b.t; });

  // Verdict: first window that is dark (sun < -12) and mostly clear (< 35%).
  _verdict = "";
  for (int h = 0; h < kHours; ++h) {
    if (_sunAlt[h] < -12 && _cloud[h] >= 0 && _cloud[h] < 35) {
      int end = h;
      while (end < kHours && _sunAlt[end] < -12 && _cloud[end] >= 0 && _cloud[end] < 35) ++end;
      struct tm a, b; time_t ta = _base + h * 3600, tb = _base + end * 3600;
      localtime_r(&ta, &a); localtime_r(&tb, &b);
      char buf[48]; snprintf(buf, sizeof(buf), "Clear & dark %02d:%02d-%02d:%02d", a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);
      _verdict = buf; break;
    }
  }
  if (_verdict.length() == 0) _verdict = _wx.valid() ? "No clear dark window in 24h" : "(clouds unavailable)";
}

void PageAgenda::tick(App& app, uint32_t nowMs) {
  if (!_dirty && nowMs - _lastDraw < 60000) return;
  _dirty = false; _lastDraw = nowMs;
  recompute();
  draw(app);
}

void PageAgenda::draw(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  g.fillRect(0, cy0, cw, ch, gTheme.bg);

  if (!_time.synced() || !_loc.active().valid) {
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(_time.synced() ? "no location" : "waiting for time sync...", cw / 2, cy0 + ch / 2);
    return;
  }

  // --- Sky Window timeline ---
  const int sx = 2, sw = cw - 4;
  const int sy = cy0 + 16, sh = 64;
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.fg, gTheme.bg);
  g.drawString("Sky Window  (next 24h)", sx, cy0 + 2);

  for (int h = 0; h < kHours; ++h) {
    int x0 = sx + sw * h / kHours, x1 = sx + sw * (h + 1) / kHours;
    int w = x1 - x0;
    g.fillRect(x0, sy, w, sh, darknessShade(_sunAlt[h]));      // darkness band
    if (_cloud[h] >= 0) {                                       // cloud heat (top)
      int gch = 20 + _cloud[h] * 180 / 100;
      g.fillRect(x0, sy, w, 14, rgb565(gch, gch, gch + 10));
    }
    if (_moonUp[h]) g.fillRect(x0, sy + sh - 4, w, 4, rgb565(70, 60, 30)); // moon-up
  }
  g.drawRect(sx, sy, sw, sh, gTheme.grid);
  // Hour ticks.
  g.setTextColor(gTheme.dim, gTheme.bg);
  for (int hh = 0; hh <= 24; hh += 6) {
    int x = sx + sw * hh / kHours;
    g.drawFastVLine(x, sy, sh, gTheme.grid);
    g.drawString(hh == 0 ? "now" : String("+") + hh, x + 1, sy + sh + 1);
  }
  // Event markers on the strip.
  for (const auto& e : _events) {
    int hoff = (int)((e.t - _base) / 3600);
    if (hoff < 0 || hoff >= kHours) continue;
    int x = sx + sw * hoff / kHours;
    g.drawFastVLine(x, sy, sh, e.launch ? gTheme.warn : gTheme.accent);
  }

  // --- Verdict ---
  int y = sy + sh + 14;
  g.setTextColor(gTheme.ok, gTheme.bg);
  g.drawString(_verdict, sx, y); y += 15;

  // --- Event list ---
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.drawString("Upcoming", sx, y); y += 13;
  for (const auto& e : _events) {
    if (y > cy0 + ch - 12) break;
    struct tm tm; time_t t = e.t; localtime_r(&t, &tm);
    char hm[8]; snprintf(hm, sizeof(hm), "%02d:%02d", tm.tm_hour, tm.tm_min);
    g.setTextDatum(textdatum_t::top_left);
    g.setTextColor(e.launch ? gTheme.warn : gTheme.accent, gTheme.bg);
    g.drawString(hm, sx, y);
    g.setTextColor(gTheme.fg, gTheme.bg);
    g.drawString(e.label.substring(0, 22), sx + 40, y);
    int cl = _wx.cloudCoverAt(e.t);
    if (cl >= 0) {
      g.setTextDatum(textdatum_t::top_right);
      g.setTextColor(cl < 35 ? gTheme.ok : cl < 70 ? gTheme.accent : gTheme.dim, gTheme.bg);
      g.drawString(String(cl) + "%", cw - 4, y);
    }
    y += 13;
  }
  if (_events.empty()) { g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("no passes or launches in 24h", sx, y); }
}
