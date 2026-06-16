#include "PageAircraft.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../providers/AircraftProvider.h"
#include "../services/LocationService.h"
#include "../services/Settings.h"
#include <math.h>
#include <time.h>

static constexpr double D2R = 3.14159265358979323846 / 180.0;

void PageAircraft::onEnter(App& app) {
  _dirty = _needClear = true;
  _ap.setForeground(true);   // full-rate polling + an immediate refresh on entry
  _ap.poll();
}

void PageAircraft::onExit(App& app) {
  _ap.setForeground(false);  // drop to the 60 s background cadence
}

void PageAircraft::onData(App& app, ProviderId id) {
  if (id == ProviderId::Aircraft) {
    int n = (int)_ap.aircraft().size();
    if (_sel >= n) _sel = n - 1;
    bool empty = (n == 0);
    if (empty != _wasEmpty) { _needClear = true; _wasEmpty = empty; }  // message<->radar
  }
  _dirty = true;
}

void PageAircraft::onTouch(App& app, int x, int y) {
  if (handleRadiusTap(app, x, y)) return;          // bottom-left range badge
  if (handleGroundTap(app, x, y)) return;          // ground-filter badge (right of it)
  int n = (int)_ap.aircraft().size();
  if (n == 0) { _sel = -1; return; }
  int third = app.contentW() / 3;
  if (x < third)          { _sel = (_sel <= 0 ? n - 1 : _sel - 1); _needClear = true; }
  else if (x > 2 * third) { _sel = (_sel + 1) % n;                 _needClear = true; }
  _dirty = true;
}

bool PageAircraft::handleRadiusTap(App& app, int x, int yRel) {
  if (x > 64 || yRel < app.contentH() - 20) return false;
  int r = (int)_settings.getInt("adsbRadiusNm", 50);
  int next = (r <= 10) ? 15 : (r <= 15) ? 25 : (r <= 25) ? 50 : 10;   // 10/15/25/50
  _settings.set("adsbRadiusNm", (long)next);
  _settings.save();
  _ap.poll();                                      // refetch with the new radius
  _dirty = _needClear = true;                      // ring label changed -> relayout
  return true;
}

bool PageAircraft::handleGroundTap(App& app, int x, int yRel) {
  if (x < 68 || x > 132 || yRel < app.contentH() - 20) return false;
  bool hide = _settings.getInt("adsbHideGround", 0) != 0;
  _settings.set("adsbHideGround", (long)(hide ? 0 : 1));
  _settings.save();
  _ap.poll();                                      // refetch / re-filter
  _dirty = _needClear = true;
  return true;
}

void PageAircraft::drawRadiusBadge(App& app) {
  auto& g = app.display().gfx();
  int y = app.contentY() + app.contentH() - 16;
  g.fillRect(4, y, 56, 14, gTheme.grid);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(gTheme.fg, gTheme.grid);
  g.setTextSize(1);
  g.drawString(String((int)_ap.radiusNm()) + " nm", 8, y + 7);
}

void PageAircraft::drawGroundBadge(App& app) {
  auto& g = app.display().gfx();
  int y = app.contentY() + app.contentH() - 16;
  bool hide = _ap.hideGround();
  g.fillRect(64, y, 64, 14, gTheme.grid);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(hide ? gTheme.dim : gTheme.fg, gTheme.grid);   // dim when filtered out
  g.setTextSize(1);
  g.drawString(hide ? "gnd: off" : "gnd: on", 68, y + 7);
}

void PageAircraft::tick(App& app, uint32_t nowMs) {
  if (!_dirty && nowMs - _lastDraw < 1000) return;
  _dirty = false;
  _lastDraw = nowMs;
  draw(app);
}

void PageAircraft::drawMessage(App& app, const char* msg) {
  auto& g = app.display().gfx();
  g.fillRect(0, app.contentY(), app.contentW(), app.contentH(), gTheme.bg);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.setTextSize(1);
  g.drawString(msg, app.contentW() / 2, app.contentY() + app.contentH() / 2);
}

void PageAircraft::draw(App& app) {
  if (!_loc.active().valid) { drawMessage(app, "no location"); return; }
  const auto& list = _ap.aircraft();
  if (list.empty()) {
    drawMessage(app, _ap.status() == ProviderStatus::Error ? "feed unavailable"
                  : _ap.status() == ProviderStatus::Loading ? "scanning..."
                  : _ap.hideGround() ? "no airborne aircraft in range"
                  : "no aircraft in range");
    drawRadiusBadge(app);    // keep the badges tappable so the user can widen
    drawGroundBadge(app);    // range or re-enable ground traffic from here
    return;
  }
  if (_sel >= (int)list.size()) _sel = list.size() - 1;

  auto& g = app.display().gfx();
  const int cw = app.contentW(), ch = app.contentH(), cy0 = app.contentY();
  if (_needClear) { g.fillRect(0, cy0, cw, ch, gTheme.bg); _needClear = false; }

  // Radar on the left. Clear just the circle's bbox each tick (blips move);
  // the info column on the right redraws in place (padded) so it stays stable.
  int size = min(ch - 8, cw / 2 - 8);
  int R = size / 2 - 12;
  int cx = 8 + R + 8, cy = cy0 + ch / 2;
  float maxR = _ap.radiusNm();
  g.fillRect(cx - R - 4, cy - R - 10, 2 * R + 8, 2 * R + 20, gTheme.bg);

  g.drawCircle(cx, cy, R, gTheme.grid);
  g.drawCircle(cx, cy, R / 2, gTheme.grid);
  g.drawFastHLine(cx - R, cy, 2 * R, gTheme.grid);
  g.drawFastVLine(cx, cy - R, 2 * R, gTheme.grid);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.setTextDatum(textdatum_t::middle_center);
  g.drawString("N", cx, cy - R - 6);
  g.drawString(String((int)maxR) + "nm", cx + R - 8, cy - 6);

  for (int i = 0; i < (int)list.size(); ++i) {
    const Aircraft& a = list[i];
    float rr = a.distNm / maxR * R; if (rr > R) rr = R;
    int ax = cx + (int)round(rr * sin(a.bearingDeg * D2R));
    int ay = cy - (int)round(rr * cos(a.bearingDeg * D2R));
    Color c = (i == _sel) ? gTheme.ok : (a.onGround ? gTheme.dim : gTheme.accent);
    // Heading tick in the track direction.
    int tx = ax + (int)round(7 * sin(a.trackDeg * D2R));
    int ty = ay - (int)round(7 * cos(a.trackDeg * D2R));
    g.drawLine(ax, ay, tx, ty, c);
    g.fillCircle(ax, ay, (i == _sel) ? 3 : 2, c);
  }

  // Info column.
  int ix = cw / 2 + 8, iy = cy0 + 6;
  g.setTextDatum(textdatum_t::top_left);
  g.setTextSize(1);
  auto line = [&](const String& s, Color col) { g.setTextColor(col, gTheme.bg); g.drawString(padRight(s, 20), ix, iy); iy += 14; };
  g.setTextColor(gTheme.fg, gTheme.bg);
  g.setTextSize(2); g.drawString("Aircraft", ix, iy); iy += 20;
  g.setTextSize(1);
  String src = String(list.size()) + " in range  " + (_ap.local() ? "local" : "cloud");
  if (_ap.status() == ProviderStatus::Stale && _ap.lastFetched()) {
    int age = (int)(time(nullptr) - _ap.lastFetched());
    if (age > 0) src += " (stale " + String(age) + "s)";
  }
  line(src, gTheme.dim);

  if (_sel >= 0 && _sel < (int)list.size()) {
    const Aircraft& a = list[_sel];
    iy += 4;
    g.setTextColor(gTheme.ok, gTheme.bg);
    g.setTextSize(2);
    g.drawString(a.flight.length() ? a.flight : a.hex, ix, iy); iy += 20;
    g.setTextSize(1);
    line(String(_sel + 1) + "/" + list.size() + "  (tap edges)", gTheme.dim);
    if (a.type.length() || a.category.length())
      line(String("type ") + (a.type.length() ? a.type : a.category), gTheme.fg);
    line(a.onGround ? String("on ground") : String("alt ") + (int)a.altFt + " ft", gTheme.fg);
    line(String("gs ") + (int)a.gsKt + " kt  trk " + (int)a.trackDeg, gTheme.fg);
    line(String("dist ") + (int)round(a.distNm) + " nm  brg " + (int)round(a.bearingDeg), gTheme.fg);
    if (a.squawk.length()) {
      bool emerg = (a.squawk == "7700" || a.squawk == "7600" || a.squawk == "7500");
      line(String("squawk ") + a.squawk, emerg ? gTheme.warn : gTheme.dim);
    }
  }

  drawRadiusBadge(app);
  drawGroundBadge(app);
}
