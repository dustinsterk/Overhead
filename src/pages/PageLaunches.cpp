#include "PageLaunches.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../providers/LaunchProvider.h"
#include "../services/TimeService.h"
#include "../services/LocationService.h"
#include "../providers/WeatherProvider.h"
#include "../astro/SolarSystem.h"
#include "../assets/Coastline.h"
#include "../assets/LaunchSites.h"
#include <algorithm>
#include <math.h>
#include <time.h>

static Color statusColor(const String& abbrev) {
  if (abbrev == "Go" || abbrev == "Success" || abbrev == "In Flight") return gTheme.ok;
  if (abbrev == "TBD" || abbrev == "TBC")                            return gTheme.dim;
  return gTheme.warn;   // Hold / Failure / Partial Failure / unknown
}

static bool precise(const String& p) {
  return p.length() == 0 || p == "Second" || p == "Minute" || p == "Hour";
}

// First token of a launch-site string (before the comma) for compact labels/chips.
static String shortSite(const String& loc) {
  int c = loc.indexOf(',');
  return c > 0 ? loc.substring(0, c) : loc;
}

// Rough "can I see it from here?" estimate: great-circle distance observer->pad plus
// the Sun's elevation at the observer at launch time. A rocket climbs into sunlight at
// altitude while the ground is dark -> the "twilight plume" visible ~1000 km (e.g.
// Vandenberg from Arizona). Honest approximation, not a guarantee. level: -1 unknown,
// 0 unlikely, 1 faint/maybe, 2 likely.
struct LaunchVis { int level = -1; String text; };
static LaunchVis launchVis(double obsLat, double obsLon, const String& location, time_t net, int cloudPct = -1) {
  LaunchVis r;
  float slat, slon; String c;
  if (!launchSiteLatLon(location, slat, slon, c) || net == 0) return r;
  const double D2R = 3.14159265358979323846 / 180.0, R = 6371.0;
  double dla = (slat - obsLat) * D2R, dlo = (slon - obsLon) * D2R;
  double a = sin(dla / 2) * sin(dla / 2) + cos(obsLat * D2R) * cos(slat * D2R) * sin(dlo / 2) * sin(dlo / 2);
  double d = R * 2 * atan2(sqrt(a), sqrt(1 - a));                          // km, great circle
  double y = sin(dlo) * cos(slat * D2R);
  double x = cos(obsLat * D2R) * sin(slat * D2R) - sin(obsLat * D2R) * cos(slat * D2R) * cos(dlo);
  double brg = atan2(y, x) / D2R; if (brg < 0) brg += 360;
  static const char* dir8[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const char* dir = dir8[((int)round(brg / 45.0)) & 7];
  double jd = (double)net / 86400.0 + 2440587.5;                          // launch instant
  double sunEl = astro::planetState(astro::Planet::Sun, jd, obsLat, obsLon).elDeg;
  const char* v;
  if (d > 1400)         { v = "below horizon";          r.level = 0; }
  else if (sunEl >= 0)  { v = (d < 150) ? "daytime, only if near" : "unlikely (daytime)"; r.level = d < 150 ? 1 : 0; }
  else if (sunEl > -18) { v = "likely - twilight plume"; r.level = 2; }    // sunlit rocket, dark sky
  else                  { v = (d < 500) ? "maybe (night)" : "faint (night, far)"; r.level = d < 500 ? 1 : 1; }
  char b[56]; snprintf(b, sizeof(b), "%dkm %s - %s", (int)round(d), dir, v);
  r.text = b;
  if (cloudPct >= 0) {                                                     // cloud at launch time
    if (cloudPct >= 70) { r.text += "  clouded " + String(cloudPct) + "%"; r.level = 0; }
    else { r.text += "  " + String(cloudPct) + "% cld"; if (cloudPct >= 40 && r.level == 2) r.level = 1; }
  }
  return r;
}

// Cycle "" (all) -> vals[0] -> vals[1] -> ... -> "" through a distinct-value list.
static String cycleVal(const std::vector<String>& vals, const String& cur) {
  if (cur.length() == 0) return vals.empty() ? String("") : vals[0];
  for (size_t i = 0; i < vals.size(); ++i)
    if (vals[i] == cur) return (i + 1 < vals.size()) ? vals[i + 1] : String("");
  return String("");
}

// Short T-minus/T-plus string ("3d", "02:15", "T+05:01") for compact readouts.
static String tMinus(time_t net, time_t now) {
  long s = (long)net - (long)now;
  bool past = s < 0; if (past) s = -s;
  char b[16];
  if (s >= 86400) snprintf(b, sizeof(b), "%s%ldd %02ldh", past ? "T+" : "T-", s / 86400, (s % 86400) / 3600);
  else            snprintf(b, sizeof(b), "%s%02ld:%02ld",  past ? "T+" : "T-", s / 3600, (s % 3600) / 60);
  return b;
}

// Build the visible launch list: fixed 7-day window, NET-TBD always hidden, then
// the active site/company filters. Also collects the distinct sites/companies in
// the window so the chips can cycle through what's actually upcoming.
void PageLaunches::rebuildFilter() {
  static const long kWin[4] = { 86400L, 604800L, 2592000L, 0L };   // 24h, 7d, 30d, all
  long win = kWin[_winIdx];
  time_t now = time(nullptr);
  const auto& list = _lp.launches();
  auto inWindow = [&](const Launch& l) {
    if (l.net == 0) return !_hideTbd;             // NET-TBD: shown only if TBD not hidden
    long dt = (long)l.net - (long)now;
    if (dt < -3600) return false;                 // already launched (keep just-launched 1h)
    return _winIdx == 3 || dt <= win;             // "all" = any future, else within window
  };

  _sites.clear(); _orgs.clear();
  for (const auto& l : list) {
    if (!inWindow(l)) continue;
    if (l.location.length() && std::find(_sites.begin(), _sites.end(), l.location) == _sites.end()) _sites.push_back(l.location);
    if (l.provider.length() && std::find(_orgs.begin(),  _orgs.end(),  l.provider) == _orgs.end())  _orgs.push_back(l.provider);
  }
  // Drop a selected filter value that's no longer present.
  if (_siteVal.length() && std::find(_sites.begin(), _sites.end(), _siteVal) == _sites.end()) _siteVal = "";
  if (_orgVal.length()  && std::find(_orgs.begin(),  _orgs.end(),  _orgVal)  == _orgs.end())  _orgVal = "";

  _filtered.clear();
  for (int i = 0; i < (int)list.size(); ++i) {
    const Launch& l = list[i];
    if (!inWindow(l)) continue;
    if (_siteVal.length() && l.location != _siteVal) continue;
    if (_orgVal.length()  && l.provider != _orgVal)  continue;
    if (_visOnly && _loc.active().valid &&
        launchVis(_loc.active().lat, _loc.active().lon, l.location, l.net, _wx.cloudCoverAt(l.net)).level < 1) continue;
    _filtered.push_back(i);
  }
  if (_sel >= (int)_filtered.size()) _sel = _filtered.empty() ? 0 : (int)_filtered.size() - 1;
}

void PageLaunches::cycleView(int) { _map = !_map; _needClear = _dirty = true; }

void PageLaunches::onEnter(App& app) {
  rebuildFilter();
  String f = app.takeFocus();                  // Agenda tap -> select the exact launch
  if (f.length()) focusLaunch(f);
  _dirty = _needClear = true;
}

void PageLaunches::focusLaunch(const String& name) {
  const auto& list = _lp.launches();
  for (int k = 0; k < (int)_filtered.size(); ++k)
    if (list[_filtered[k]].name == name) { _sel = k; _needClear = _dirty = true; return; }
}

void PageLaunches::onData(App& app, ProviderId id) {
  if (id == ProviderId::Launch) rebuildFilter();
  _dirty = _needClear = true;
}

void PageLaunches::onTouch(App& app, int x, int y) {
  const int cw = app.contentW(), ch = app.contentH();
  const int u = app.ui();
  if (y >= ch - 18 * u) {                              // bottom filter chips
    int xs = 120 * u, ws = (cw - xs - 2 * u) / 2;
    if      (x < 38 * u)   _winIdx  = (_winIdx + 1) % 4;       // time window
    else if (x < 80 * u)   _hideTbd = !_hideTbd;               // TBD show/hide
    else if (x < xs)       _visOnly = !_visOnly;               // only-visible filter
    else if (x < xs + ws)  _siteVal = cycleVal(_sites, _siteVal);
    else                   _orgVal  = cycleVal(_orgs,  _orgVal);
    rebuildFilter(); _sel = 0; _needClear = _dirty = true; return;
  }
  int third = cw / 3;
  if (x >= third && x <= 2 * third) { _map = !_map; _needClear = _dirty = true; return; }  // centre toggles view
  int n = (int)_filtered.size();
  if (n == 0) return;
  if (x < third) _sel = (_sel - 1 + n) % n;
  else           _sel = (_sel + 1) % n;
  _needClear = !_map;                                  // map repaints fully anyway
  _dirty = true;
}

String PageLaunches::gridStatus() {
  const auto& list = _lp.launches();
  time_t now = time(nullptr), best = 0; const Launch* bl = nullptr;
  for (const auto& l : list) if (l.net > now && (best == 0 || l.net < best)) { best = l.net; bl = &l; }
  if (!bl) return String();
  String nm = bl->mission.length() ? bl->mission : bl->name;
  nm.trim(); if (nm.length() > 10) nm = nm.substring(0, 10);
  return tMinus(best, now) + " " + nm;
}

bool PageLaunches::autoAdvance(App&) {
  int n = (int)_filtered.size();                       // tour the filtered launches
  if (n <= 0) return true;
  _sel = (_sel + 1) % n; _needClear = !_map; _dirty = true;
  return _sel == 0;                                    // wrapped = full cycle
}

void PageLaunches::tick(App& app, uint32_t nowMs) {
  uint32_t period = _map ? 60000 : 1000;               // card counts down each second; map is static
  if (!_dirty && nowMs - _lastDraw < period) return;
  _dirty = false;
  _lastDraw = nowMs;
  draw(app);
}

void PageLaunches::drawMessage(App& app, const char* msg) {
  auto& g = app.display().gfx();
  g.fillRect(0, app.contentY(), app.contentW(), app.contentH(), gTheme.bg);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.setTextSize(1);
  g.drawString(msg, app.contentW() / 2, app.contentY() + app.contentH() / 2);
}

void PageLaunches::draw(App& app) {
  rebuildFilter();
  const auto& list = _lp.launches();
  if (_filtered.empty()) {
    drawMessage(app, list.empty()
                  ? (_lp.status() == ProviderStatus::Error ? "launch fetch failed"
                     : _lp.status() == ProviderStatus::Loading ? "loading launches..." : "no upcoming launches")
                  : "none match the filters");
    drawChips(app);
    return;
  }
  if (_sel >= (int)_filtered.size()) _sel = 0;
  if (_map) drawMap(app); else drawCard(app);
  drawChips(app);
}

void PageLaunches::drawCard(App& app) {
  const auto& list = _lp.launches();
  const Launch& l = list[_filtered[_sel]];

  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  const int u = app.ui();
  if (_needClear) { g.fillRect(0, cy0, cw, ch, gTheme.bg); _needClear = false; }
  time_t now = time(nullptr);

  // Status pill (top-right).
  g.setTextDatum(textdatum_t::top_right);
  g.setTextSize(u);
  g.setTextColor(statusColor(l.statusAbbrev), gTheme.bg);
  g.drawString(l.statusName.length() ? l.statusName : l.statusAbbrev, cw - 6 * u, cy0 + 4 * u);
  if (_lp.status() == ProviderStatus::Stale) {
    g.setTextColor(gTheme.warn, gTheme.bg);
    g.drawString("stale", cw - 6 * u, cy0 + 16 * u);
  }

  int x0 = 6 * u, y = cy0 + 4 * u;
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.setTextSize(2 * u);
  g.drawString(l.name.substring(0, 16), x0, y); y += 22 * u;

  g.setTextSize(u);
  auto line = [&](const String& s, Color c) { if (s.length()) { g.setTextColor(c, gTheme.bg); g.drawString(s, x0, y); y += 13 * u; } };
  // Where it's launching from — surfaced prominently (accent), with country.
  String ctry = launchSiteCountry(l.location);
  line(String("@ ") + l.location, gTheme.accent);
  line(l.provider + (l.vehicle.length() ? "  -  " + l.vehicle : String()) + (ctry.length() ? "  (" + ctry + ")" : String()), gTheme.fg);
  line(l.pad + (l.mission.length() ? "  -  " + l.mission : String()), gTheme.dim);
  line(String(_sel + 1) + "/" + _filtered.size() + (_lp.usingFallback() ? "  (RLL)" : ""), gTheme.dim);

  // T-minus right under the index line (left-aligned); the list fills the rest.
  g.setTextDatum(textdatum_t::top_left);
  y += 2 * u;
  if (precise(l.netPrecision)) {
    long s = (long)l.net - (long)now;
    bool past = s < 0; if (past) s = -s;
    long d = s / 86400; s %= 86400; long h = s / 3600; s %= 3600; long m = s / 60, sec = s % 60;
    char b[24];
    if (d > 0) snprintf(b, sizeof(b), "%s%ldd %02ld:%02ld", past ? "T+" : "T-", d, h, m);
    else       snprintf(b, sizeof(b), "%s%02ld:%02ld:%02ld", past ? "T+" : "T-", h, m, sec);
    g.setTextColor(past ? gTheme.warn : gTheme.fg, gTheme.bg);
    g.setTextSize(3 * u); g.drawString(padRight(b, 12), x0, y);
    { struct tm tm; time_t t = l.net; localtime_r(&t, &tm);   // local launch day + time (small)
      char d1[16], d2[16];
      strftime(d1, sizeof(d1), "%a %b %d", &tm); strftime(d2, sizeof(d2), "%H:%M", &tm);
      g.setTextSize(u); g.setTextColor(gTheme.dim, gTheme.bg); g.setTextDatum(textdatum_t::top_right);
      g.drawString(d1, cw - 6 * u, y + 3 * u); g.drawString(String(d2) + " local", cw - 6 * u, y + 15 * u);
      g.setTextDatum(textdatum_t::top_left);
    }
    y += 28 * u;
  } else {
    struct tm tm; time_t t = l.net; localtime_r(&t, &tm);
    char b[24]; strftime(b, sizeof(b), "~ %b %d", &tm);
    g.setTextColor(gTheme.fg, gTheme.bg); g.setTextSize(2 * u);
    g.drawString(b, x0, y); y += 20 * u;
    g.setTextSize(u); g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(String("precision: ") + l.netPrecision, x0, y); y += 14 * u;
  }

  // "Can I see it from here?" — distance/direction + twilight-plume likelihood.
  if (_loc.active().valid) {
    LaunchVis vis = launchVis(_loc.active().lat, _loc.active().lon, l.location, l.net, _wx.cloudCoverAt(l.net));
    if (vis.level >= 0) {
      Color vc = vis.level == 2 ? gTheme.ok : vis.level == 1 ? gTheme.warn : gTheme.dim;
      g.setTextDatum(textdatum_t::top_left); g.setTextSize(u);
      g.setTextColor(vc, gTheme.bg);
      g.drawString(("see: " + vis.text).substring(0, (cw - 2 * x0) / (6 * u)), x0, y); y += 13 * u;
    }
  }

  // Upcoming list fills the remaining space (above the chip row).
  g.drawFastHLine(x0, y, cw - 2 * x0, gTheme.grid); y += 4 * u;
  g.setTextSize(u);
  for (int fi = 0; fi < (int)_filtered.size() && y < cy0 + ch - 17 * u; ++fi) {  // fill down to the chips
    if (fi == _sel) continue;
    const Launch& up = list[_filtered[fi]];
    long s = (long)up.net - (long)now; if (s < 0) s = 0;
    char tm[12];
    if (s >= 86400) snprintf(tm, sizeof(tm), "%ldd", s / 86400);
    else snprintf(tm, sizeof(tm), "%02ld:%02ld", s / 3600, (s % 3600) / 60);
    int nx = x0;                                      // mark a potentially-visible launch
    if (_loc.active().valid) {
      LaunchVis uv = launchVis(_loc.active().lat, _loc.active().lon, up.location, up.net, _wx.cloudCoverAt(up.net));
      if (uv.level >= 1) { g.fillCircle(x0 + 2 * u, y + 5 * u, 2 * u, uv.level == 2 ? gTheme.ok : gTheme.warn); nx = x0 + 8 * u; }
    }
    g.setTextDatum(textdatum_t::top_left);
    g.setTextColor(gTheme.fg, gTheme.bg);
    int nameMax = (cw - nx - 48 * u - x0) / (6 * u);  // fill up to the time cell
    g.drawString(up.name.substring(0, nameMax), nx, y);
    g.fillRect(cw - x0 - 44 * u, y, 44 * u, 12 * u, gTheme.bg);   // clear time cell (right-aligned, shrinks)
    g.setTextDatum(textdatum_t::top_right);
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(tm, cw - x0, y);
    y += 13 * u;
  }
}

// World map (equirectangular) with a marker at each upcoming launch site; the
// selected rocket's site is ringed + labelled. Side-tap cycles the selection.
void PageLaunches::drawMap(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  const int u = app.ui();
  g.fillRect(0, cy0, cw, ch, gTheme.bg);            // map repaints fully each time
  _needClear = false;

  const auto& list = _lp.launches();
  const Launch& sel = list[_filtered[_sel]];
  time_t now = time(nullptr);

  // Header: selected launch (name + provider/country + T-minus + index).
  g.setTextDatum(textdatum_t::top_left);
  g.setTextSize(u);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.drawString(sel.name.substring(0, 32), 4 * u, cy0 + 2 * u);
  String ctry = launchSiteCountry(sel.location);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.drawString(shortSite(sel.location) + (ctry.length() ? ", " + ctry : String()), 4 * u, cy0 + 14 * u);
  g.setTextDatum(textdatum_t::top_right);
  g.setTextColor(gTheme.fg, gTheme.bg);
  g.drawString(tMinus(sel.net, now) + "  " + String(_sel + 1) + "/" + _filtered.size(), cw - 4 * u, cy0 + 2 * u);
  if (sel.net) {                                   // launch time in the user's local zone
    struct tm tm; time_t t = sel.net; localtime_r(&t, &tm);
    char lt[16]; strftime(lt, sizeof(lt), "%a %H:%M", &tm);
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(lt, cw - 4 * u, cy0 + 14 * u);
  }

  // Label bands scale (26 top / 18 bottom); the map fills the middle at native resolution.
  const int my = cy0 + 26 * u, mh = ch - 26 * u - 18 * u, mx = 0, mw = cw;
  auto px = [&](float lon) { return mx + (int)round((lon + 180.0f) / 360.0f * mw); };
  auto py = [&](float lat) { return my + (int)round((90.0f - lat) / 180.0f * mh); };

  g.drawRect(mx, my, mw, mh, gTheme.grid);
  g.drawFastHLine(mx, py(0.0f), mw, gTheme.dim);    // equator

  // Coastlines + borders (Natural Earth; 0.1-degree units, shared with Satellites).
  for (int i = 1; i < kCoastlineCount; ++i) {
    const CoastPt& a = kCoastline[i - 1];
    const CoastPt& b = kCoastline[i];
    if (a.lon == 9999 || b.lon == 9999) continue;   // pen-up
    if (abs(a.lon - b.lon) > 1800) continue;        // seam
    g.drawLine(px(a.lon / 10.0f), py(a.lat / 10.0f), px(b.lon / 10.0f), py(b.lat / 10.0f), gTheme.dim);
  }

  // Observer location: green circle + black centre dot (matches the Satellites map).
  if (_loc.active().valid) {
    int ox = px(_loc.active().lon), oy = py(_loc.active().lat);
    g.fillCircle(ox, oy, 4 * u, gTheme.ok);
    g.fillCircle(ox, oy, 1 * u, 0x0000);
  }

  // Markers for every filtered launch's site; selected one ringed + labelled last.
  int selX = -1, selY = -1; bool selKnown = false;
  for (int fi = 0; fi < (int)_filtered.size(); ++fi) {
    const Launch& l = list[_filtered[fi]];
    float lat, lon; String c;
    if (!launchSiteLatLon(l.location, lat, lon, c)) continue;   // unknown site -> skip dot
    int sx = px(lon), sy = py(lat);
    if (fi == _sel) { selX = sx; selY = sy; selKnown = true; continue; }   // draw selected on top
    g.fillCircle(sx, sy, 2 * u, gTheme.warn);
  }
  if (selKnown) {
    // Approx launch-corridor arrow from the pad (per-site azimuth; see launchSiteAz).
    int az = launchSiteAz(sel.location);
    if (az != 0) {
      float th = az * 3.14159265f / 180.0f, dx = sinf(th), dy = -cosf(th);   // N=up, E=right
      int ex = selX + (int)round(28 * u * dx), ey = selY + (int)round(28 * u * dy);
      g.drawLine(selX, selY, ex, ey, gTheme.accent);
      float px2 = -dx, py2 = -dy, qx = dy, qy = -dx;                         // back + perpendicular
      g.drawLine(ex, ey, ex + (int)round((7 * px2 + 4 * qx) * u), ey + (int)round((7 * py2 + 4 * qy) * u), gTheme.accent);
      g.drawLine(ex, ey, ex + (int)round((7 * px2 - 4 * qx) * u), ey + (int)round((7 * py2 - 4 * qy) * u), gTheme.accent);
      g.setTextDatum(textdatum_t::bottom_left);
      g.setTextColor(gTheme.dim, gTheme.bg);
      g.drawString("-> approx launch corridor", mx + 4 * u, my + mh - 2 * u);
    }
    g.fillCircle(selX, selY, 4 * u, gTheme.ok);
    g.drawCircle(selX, selY, 7 * u, gTheme.ok);
    g.setTextColor(gTheme.ok, gTheme.bg);
    String lbl = shortSite(sel.location);
    int maxRight = (cw - (selX + 9 * u)) / (6 * u), maxLeft = (selX - 9 * u) / (6 * u);   // chars that fit each side
    if (maxRight >= maxLeft) {
      g.setTextDatum(textdatum_t::bottom_left);
      g.drawString(lbl.substring(0, maxRight), selX + 9 * u, selY - 1 * u);
    } else {
      g.setTextDatum(textdatum_t::bottom_right);
      g.drawString(lbl.substring(0, maxLeft), selX - 9 * u, selY - 1 * u);
    }
  } else {
    g.setTextColor(gTheme.warn, gTheme.bg);
    g.setTextDatum(textdatum_t::bottom_left);
    g.drawString("site not mapped", mx + 4 * u, my + mh - 2 * u);
  }

  // Observer marker + sight line to the selected pad, coloured by visibility.
  if (_loc.active().valid) {
    int ox = px((float)_loc.active().lon), oy = py((float)_loc.active().lat);
    if (selKnown) {
      LaunchVis v = launchVis(_loc.active().lat, _loc.active().lon, sel.location, sel.net, _wx.cloudCoverAt(sel.net));
      if (v.level >= 0) {
        Color vc = v.level == 2 ? gTheme.ok : v.level == 1 ? gTheme.warn : gTheme.dim;
        g.drawLine(ox, oy, selX, selY, vc);
        g.setTextDatum(textdatum_t::bottom_right); g.setTextColor(vc, gTheme.bg);
        g.drawString(String("visible: ") + (v.level == 2 ? "likely" : v.level == 1 ? "faint" : "unlikely"), cw - 4 * u, my + mh - 2 * u);
      }
    }
    g.drawFastHLine(ox - 3 * u, oy, 7 * u, gTheme.accent);   // observer crosshair = "you"
    g.drawFastVLine(ox, oy - 3 * u, 7 * u, gTheme.accent);
    g.setTextDatum(textdatum_t::top_left); g.setTextColor(gTheme.accent, gTheme.bg);
    g.drawString("you", ox + 4 * u, oy - 4 * u);
  }
}

void PageLaunches::drawChips(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW();
  const int u = app.ui();
  int y = app.contentY() + app.contentH() - 16 * u;
  g.setTextSize(u);
  g.setTextDatum(textdatum_t::middle_left);
  auto chip = [&](int x0, int w, const String& label, Color fg) {
    g.fillRect(x0, y, w - 2 * u, 14 * u, gTheme.grid);
    g.setTextColor(fg, gTheme.grid);
    g.drawString(label.substring(0, (w - 8 * u) / (6 * u)), x0 + 4 * u, y + 7 * u);
  };
  static const char* kWinL[4] = { "24h", "7d", "30d", "all" };
  int xs = 120 * u, ws = (cw - xs - 2 * u) / 2;
  chip(2 * u,  36 * u, kWinL[_winIdx], _winIdx == 3 ? gTheme.fg : gTheme.ok);   // time window
  chip(40 * u, 40 * u, _hideTbd ? "-TBD" : "+TBD", _hideTbd ? gTheme.fg : gTheme.warn);
  chip(82 * u, 36 * u, "vis", _visOnly ? gTheme.ok : gTheme.fg);               // only-visible filter
  String s = _siteVal.length() ? shortSite(_siteVal) : String("all");
  String o = _orgVal.length()  ? _orgVal             : String("all");
  chip(xs, ws, String("site:") + s, _siteVal.length() ? gTheme.ok : gTheme.fg);
  chip(xs + ws, ws, String("org:") + o, _orgVal.length() ? gTheme.ok : gTheme.fg);
}
