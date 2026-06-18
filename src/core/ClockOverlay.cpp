#include "ClockOverlay.h"
#include "App.h"
#include "Theme.h"
#include "../hal/Display.h"
#include "../services/TimeService.h"
#include "../services/Settings.h"
#include <Arduino.h>
#include <time.h>
#include <math.h>

static constexpr int      kStripH = 14;          // built-in chip strip at the bottom of the box
static constexpr uint16_t kKey    = 0xF81F;      // sprite transparent key (magenta; never used by the clock)

void ClockOverlay::begin() {
  _use24 = _settings.getBool("clk24", true);
  _ball  = _settings.getBool("clkBall", false);
}

void ClockOverlay::layoutBox(App& app) {
  if (_ball) { _bw = 88;  _gh = 84; }      // analog disc + strip
  else       { _bw = 128; _gh = 42; }      // time + date + strip
  _bh = _gh + kStripH;
  if (_bw > app.contentW()) _bw = app.contentW();
  if (_bh > app.contentH()) _bh = app.contentH();
}

void ClockOverlay::cornerPos(App& app, int corner, int& x, int& y) {
  const int m = 3, cw = app.contentW(), y0 = app.contentY(), ch = app.contentH();
  bool right  = (corner == 1 || corner == 3);
  bool bottom = (corner == 3);                  // (corner 2 / lower-left is avoided)
  x = right  ? cw - _bw - m : m;
  y = bottom ? y0 + ch - _bh - m : y0 + m;
}

int ClockOverlay::pickCorner() {
  static const int kCorners[] = {0, 1, 3};      // skip lower-left (chip row lives there)
  int c;
  do { _rng = _rng * 1664525u + 1013904223u; c = kCorners[(_rng >> 24) % 3]; } while (c == _corner);
  return c;
}

void ClockOverlay::toggle(App& app) {
  _on = !_on;
  if (_on) { begin(); _corner = 3; _firstFrame = true; _shownMin = -2; }
  else {
    if (_spr) { delete (lgfx::LGFX_Sprite*)_spr; _spr = nullptr; _sprW = _sprH = 0; }
    app.repaintActive();                        // clock gone -> restore the page cleanly
  }
}

void ClockOverlay::prepare(App& app, uint32_t now, bool live) {
  layoutBox(app);
  int next = _corner;
  if (live) next = 3;                            // live pages: parked lower-right
  else if (_firstFrame || now - _cornerMs >= 10000) { next = pickCorner(); _cornerMs = now; }

  bool hop = _firstFrame || (next != _corner);
  _corner = next; _firstFrame = false;
  if (hop) app.repaintActive();                  // clean full repaint; page redraws this same frame
}

void ClockOverlay::renderSprite(App& app) {
  auto& gfx = app.display().gfx();
  auto* spr = (lgfx::LGFX_Sprite*)_spr;
  if (spr && (_sprW != _bw || _sprH != _bh)) { delete spr; spr = nullptr; _spr = nullptr; }
  if (!spr) {
    spr = new lgfx::LGFX_Sprite(&gfx);
    spr->setColorDepth(16);
    if (!spr->createSprite(_bw, _bh)) { delete spr; _spr = nullptr; return; }   // OOM -> direct fallback
    _spr = spr; _sprW = _bw; _sprH = _bh;
  }
  spr->fillScreen(kKey);                          // key = transparent (rounded corners show the page)
  drawClock(*spr, 0, 0);
  drawChips(*spr, 0, 0);
  struct tm tm; time_t now = time(nullptr); localtime_r(&now, &tm);
  _shownMin = tm.tm_min; _shownBall = _ball; _shown24 = _use24;
}

void ClockOverlay::stamp(App& app) {
  auto& gfx = app.display().gfx();
  int bx, by; cornerPos(app, _corner, bx, by);

  struct tm tm; time_t now = time(nullptr); localtime_r(&now, &tm);
  bool changed = !_spr || _sprW != _bw || _sprH != _bh ||
                 _shownMin != tm.tm_min || _shownBall != _ball || _shown24 != _use24;
  if (changed) renderSprite(app);

  if (_spr) ((lgfx::LGFX_Sprite*)_spr)->pushSprite(bx, by, kKey);
  else {                                          // sprite OOM: draw straight to the panel
    gfx.startWrite(); drawClock(gfx, bx, by); drawChips(gfx, bx, by); gfx.endWrite();
  }

  const int sy = (by + _gh) - app.contentY(), hw = _bw / 2;   // chip hit rects (content-relative)
  _fmtX = bx;      _fmtY = sy; _fmtW = hw - 1;   _fmtH = kStripH;
  _styX = bx + hw; _styY = sy; _styW = _bw - hw; _styH = kStripH;
}

// --- drawing (into a sprite at 0,0, or straight to the panel at x,y) --------
void ClockOverlay::drawClock(lgfx::LovyanGFX& g, int x, int y) {
  if (_ball) drawAnalog(g, x, y); else drawDigits(g, x, y);
}

void ClockOverlay::drawDigits(lgfx::LovyanGFX& g, int x, int y) {
  time_t now = time(nullptr); struct tm tm; localtime_r(&now, &tm);
  char hm[8]; const char* ampm = "";
  if (!_time.synced()) { strncpy(hm, "--:--", sizeof(hm)); }
  else if (_use24) { strftime(hm, sizeof(hm), "%H:%M", &tm); }
  else {
    int h = tm.tm_hour % 12; if (h == 0) h = 12;
    snprintf(hm, sizeof(hm), "%d:%02d", h, tm.tm_min);
    ampm = (tm.tm_hour < 12) ? "AM" : "PM";
  }
  char date[16]; strftime(date, sizeof(date), "%a %b %d", &tm);

  g.fillRoundRect(x, y, _bw, _bh, 6, gTheme.bg);     // opaque card -> legible over anything
  g.drawRoundRect(x, y, _bw, _bh, 6, gTheme.grid);

  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(gTheme.fg, gTheme.bg);
  g.setTextSize(4);
  g.drawString(hm, x + _bw / 2, y + 16);

  char line2[24];
  if (_use24) snprintf(line2, sizeof(line2), "%s", date);
  else        snprintf(line2, sizeof(line2), "%s  %s", ampm, date);
  g.setTextSize(1);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.drawString(line2, x + _bw / 2, y + _gh - 6);
}

void ClockOverlay::drawAnalog(lgfx::LovyanGFX& g, int x, int y) {
  time_t now = time(nullptr); struct tm tm; localtime_r(&now, &tm);
  int cx = x + _bw / 2, cy = y + _gh / 2, r = _gh / 2 - 4;

  g.fillCircle(cx, cy, r, gTheme.bg);           // solid face -> legible "clock ball"
  g.drawCircle(cx, cy, r, gTheme.accent);
  g.drawCircle(cx, cy, r - 1, gTheme.accent);   // 2px rim
  for (int t = 0; t < 12; ++t) {                // hour ticks (bigger dots on the quarters)
    float a = t * (float)M_PI / 6.0f;
    int x0 = cx + (int)lroundf((r - 3) * sinf(a)), y0 = cy - (int)lroundf((r - 3) * cosf(a));
    g.fillCircle(x0, y0, (t % 3 == 0) ? 2 : 1, gTheme.dim);
  }
  char date[12]; strftime(date, sizeof(date), "%b %d", &tm);        // date complication on the face
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextSize(1);
  g.setTextColor(gTheme.dim, gTheme.bg);
  g.drawString(date, cx, cy + r / 2);
  float ma = tm.tm_min * (float)M_PI / 30.0f;                       // minute hand
  float ha = (tm.tm_hour % 12 + tm.tm_min / 60.0f) * (float)M_PI / 6.0f;  // hour hand
  g.drawWideLine(cx, cy, cx + (int)lroundf(r * 0.86f * sinf(ma)), cy - (int)lroundf(r * 0.86f * cosf(ma)), 3, gTheme.fg);
  g.drawWideLine(cx, cy, cx + (int)lroundf(r * 0.55f * sinf(ha)), cy - (int)lroundf(r * 0.55f * cosf(ha)), 4, gTheme.fg);
  g.fillCircle(cx, cy, 3, gTheme.accent);
}

void ClockOverlay::drawChips(lgfx::LovyanGFX& g, int x, int y) {
  const int sy = y + _gh, hw = _bw / 2;          // strip inside the box, split in two
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextSize(1);
  g.fillRect(x, sy, hw - 1, kStripH, gTheme.grid);          // left: time format
  g.setTextColor(gTheme.fg, gTheme.grid);
  g.drawString(_use24 ? "24h" : "AMPM", x + (hw - 1) / 2, sy + kStripH / 2);
  g.fillRect(x + hw, sy, _bw - hw, kStripH, gTheme.grid);   // right: digits/analog
  g.setTextColor(gTheme.fg, gTheme.grid);
  g.drawString(_ball ? "clk" : "123", x + hw + (_bw - hw) / 2, sy + kStripH / 2);
}

bool ClockOverlay::handleTap(App& app, int xRel, int yRel) {
  auto hit = [&](int hx, int hy, int hw, int hh) {
    return xRel >= hx - 4 && xRel <= hx + hw + 4 && yRel >= hy - 4 && yRel <= hy + hh + 4;
  };
  if (hit(_fmtX, _fmtY, _fmtW, _fmtH)) {        // 24h <-> AM/PM
    _use24 = !_use24; _settings.set("clk24", _use24); _settings.save();
    _shownMin = -2;                             // force a sprite re-render
    return true;
  }
  if (hit(_styX, _styY, _styW, _styH)) {        // digits <-> analog (box size changes)
    _ball = !_ball; _settings.set("clkBall", _ball); _settings.save();
    layoutBox(app); _shownMin = -2;
    app.repaintActive();                        // clear the old footprint cleanly
    return true;
  }
  return false;
}
