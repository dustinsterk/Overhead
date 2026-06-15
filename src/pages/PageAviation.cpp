#include "PageAviation.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../providers/AviationWxProvider.h"
#include "../providers/SoundingProvider.h"
#include "../providers/HazardProvider.h"
#include "../services/LocationService.h"
#include <math.h>

static Color catColor(const String& c) {
  if (c == "VFR")  return gTheme.ok;
  if (c == "MVFR") return gTheme.accent;
  return gTheme.warn;   // IFR / LIFR
}

static int drawWrapped(LGFX& g, const String& text, int x, int y, int maxChars, int maxLines, Color c) {
  g.setTextColor(c, gTheme.bg);
  g.setTextDatum(textdatum_t::top_left);
  int line = 0, i = 0, n = text.length();
  while (i < n && line < maxLines) {
    int end = i + maxChars;
    if (end >= n) end = n; else { int sp = text.lastIndexOf(' ', end); if (sp > i) end = sp; }
    g.drawString(text.substring(i, end), x, y);
    y += 11; line++;
    i = end + (end < n && text[end] == ' ' ? 1 : 0);
  }
  return y;
}

void PageAviation::onData(App& app, ProviderId id) {
  int n = (int)_wx.stations().size();
  if (_sel >= n) _sel = n ? n - 1 : 0;
  bool empty = (n == 0);
  if (empty != _wasEmpty) { _wasEmpty = empty; }
  _dirty = _needClear = true;
}

void PageAviation::onTouch(App& app, int x, int y) {
  int third = app.contentW() / 3;
  if (x >= third && x <= 2 * third) {               // centre: cycle view
    _view = _view == View::Metar ? View::Sounding : _view == View::Sounding ? View::Hazards : View::Metar;
    _needClear = _dirty = true; return;
  }
  int n = (int)_wx.stations().size();
  if (n && _view == View::Metar) {
    _sel = (x < third) ? (_sel - 1 + n) % n : (_sel + 1) % n;
    _needClear = _dirty = true;
  }
}

void PageAviation::tick(App& app, uint32_t nowMs) {
  if (!_dirty && nowMs - _lastDraw < 5000) return;
  _dirty = false; _lastDraw = nowMs;
  draw(app);
}

void PageAviation::draw(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  if (_needClear) { g.fillRect(0, cy0, cw, ch, gTheme.bg); _needClear = false; }
  if (_view == View::Metar)         drawMetar(app);
  else if (_view == View::Sounding) drawSounding(app);
  else                              drawHazards(app);
}

void PageAviation::drawMetar(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  const auto& list = _wx.stations();
  if (list.empty()) {
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(!_loc.active().valid ? "no location"
               : _wx.status() == ProviderStatus::Error ? "AWC unavailable" : "loading METARs...",
                 cw / 2, cy0 + ch / 2);
    return;
  }
  if (_sel >= (int)list.size()) _sel = 0;
  const Metar& m = list[_sel];
  const int maxc = (cw - 10) / 6;
  int x0 = 6, y = cy0 + 4;

  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.setTextSize(2); g.drawString(m.icao, x0, y);
  g.setTextDatum(textdatum_t::top_right);
  g.setTextColor(catColor(m.cat), gTheme.bg); g.drawString(m.cat, cw - 6, y + 2);
  g.setTextDatum(textdatum_t::top_left); y += 22;
  g.setTextSize(1);
  auto line = [&](const String& s, Color c) { g.setTextColor(c, gTheme.bg); g.drawString(padRight(s, maxc), x0, y); y += 12; };

  line(m.name.substring(0, maxc) + "  " + (int)round(m.distNm) + "nm  [tap mid: sounding]", gTheme.dim);
  if (m.wspd >= 0) line(m.wspd == 0 ? String("wind calm")
        : String("wind ") + (m.wdir < 0 ? String("VRB") : String(m.wdir)) + "\xF7 @ " + m.wspd + " kt", gTheme.fg);
  if (m.visSm >= 0)   line(String("vis ") + String(m.visSm, 0) + " sm", gTheme.fg);
  line(m.ceilingFt >= 0 ? String("ceiling ") + m.ceilingFt + " ft" : String("ceiling: none"), gTheme.fg);
  if (m.tempC > -999) line(String("temp ") + m.tempC + "  dewpt " + m.dewpC + " C", gTheme.fg);
  if (m.altimHpa > 0) line(String("QNH ") + m.altimHpa + " hPa", gTheme.fg);
  if (m.wx.length())  line(String("wx ") + m.wx, gTheme.warn);
  line(String(_sel + 1) + "/" + list.size(), gTheme.dim);
  y += 3; g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("METAR", x0, y); y += 11;
  y = drawWrapped(g, m.raw, x0, y, maxc, 3, gTheme.fg);
  if (m.taf.length() && y < cy0 + ch - 24) {
    y += 2; g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("TAF", x0, y); y += 11;
    drawWrapped(g, m.taf, x0, y, maxc, (cy0 + ch - y) / 11, gTheme.dim);
  }
}

void PageAviation::drawSounding(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  const auto& lv = _snd.levels();
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.drawString("Sounding (Op40)  [tap mid: hazards]", 6, cy0 + 2);
  if (lv.size() < 3) {
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString(_snd.status() == ProviderStatus::Error ? "sounding unavailable" : "loading sounding...", 6, cy0 + ch / 2);
    return;
  }

  // Plot box: x = temp -40..40, y = alt 0..12 km.
  const int gx = 24, gy = cy0 + 16, gw = cw - 90, gh = ch - 24;
  const float Tmin = -40, Tmax = 40, Amax = 12000;
  auto px = [&](float T) { return gx + (int)((T - Tmin) / (Tmax - Tmin) * gw); };
  auto py = [&](float a) { return gy + gh - (int)(fminf(a, Amax) / Amax * gh); };
  g.drawRect(gx, gy, gw, gh, gTheme.grid);
  g.setTextColor(gTheme.dim, gTheme.bg);
  if (px(0) > gx && px(0) < gx + gw) g.drawFastVLine(px(0), gy, gh, gTheme.grid);  // 0 C
  for (int km = 3; km <= 12; km += 3) { int yy = py(km * 1000); g.drawFastHLine(gx, yy, gw, gTheme.grid); g.drawString(String(km), 4, yy - 3); }

  int px0t = -1, py0t = 0, px0d = -1, py0d = 0;
  for (const auto& l : lv) {
    if (l.altM > Amax) break;
    int xt = px(l.tempC), yt = py(l.altM);
    if (px0t >= 0) g.drawLine(px0t, py0t, xt, yt, gTheme.warn);
    px0t = xt; py0t = yt;
    if (l.dewpC > -900) { int xd = px(l.dewpC), yd = py(l.altM); if (px0d >= 0) g.drawLine(px0d, py0d, xd, yd, gTheme.accent); px0d = xd; py0d = yd; }
  }

  // Freezing level.
  if (_snd.freezingLevelM() > 0) {
    int yf = py(_snd.freezingLevelM());
    g.drawFastHLine(gx, yf, gw, gTheme.ok);
    g.setTextColor(gTheme.ok, gTheme.bg);
    g.setTextDatum(textdatum_t::bottom_right);
    g.drawString(String("FZL ") + (int)(_snd.freezingLevelM() * 3.281f) + "ft", cw - 2, yf);
    g.setTextDatum(textdatum_t::top_left);
  }

  // Winds aloft (nearest levels to 1/3/6/9 km).
  int wy = gy + 2; const int targets[] = {1000, 3000, 6000, 9000};
  g.setTextColor(gTheme.fg, gTheme.bg);
  for (int t : targets) {
    const SoundingLevel* best = nullptr; float bd = 1e9;
    for (const auto& l : lv) if (l.wspd >= 0) { float d = fabsf(l.altM - t); if (d < bd) { bd = d; best = &l; } }
    if (best) { char b[24]; snprintf(b, sizeof(b), "%dk %03d@%d", t / 1000, best->wdir, best->wspd); g.drawString(b, cw - 60, wy); wy += 11; }
  }
}

void PageAviation::drawHazards(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  const int maxc = (cw - 10) / 6;
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.drawString("Hazards  [tap mid: metar]", 6, cy0 + 2);
  const auto& hz = _haz.hazards();
  int y = cy0 + 16;
  if (hz.empty()) {
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString("no AIRMET/SIGMET/PIREP nearby", 6, cy0 + ch / 2);
    return;
  }
  for (const auto& h : hz) {
    if (y > cy0 + ch - 12) break;
    g.setTextColor(h.pirep ? gTheme.dim : gTheme.warn, gTheme.bg);
    y = drawWrapped(g, (h.pirep ? "PIREP " : "") + h.text, 6, y, maxc, 2, h.pirep ? gTheme.dim : gTheme.warn);
    y += 2;
  }
}
