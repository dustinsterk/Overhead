#include "PageAviation.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../providers/AviationWxProvider.h"
#include "../services/LocationService.h"

static Color catColor(const String& c) {
  if (c == "VFR")  return gTheme.ok;
  if (c == "MVFR") return gTheme.accent;
  return gTheme.warn;   // IFR / LIFR
}

// Word-wrap text to ~maxChars per line; returns the y after the last line.
static int drawWrapped(LGFX& g, const String& text, int x, int y, int maxChars, int maxLines, Color c) {
  g.setTextColor(c, gTheme.bg);
  g.setTextDatum(textdatum_t::top_left);
  int line = 0, i = 0, n = text.length();
  while (i < n && line < maxLines) {
    int end = i + maxChars;
    if (end >= n) end = n;
    else { int sp = text.lastIndexOf(' ', end); if (sp > i) end = sp; }
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
  if (empty != _wasEmpty) { _needClear = true; _wasEmpty = empty; }
  _dirty = _needClear = true;     // METAR refresh is infrequent; relayout cleanly
}

void PageAviation::onTouch(App& app, int x, int y) {
  int n = (int)_wx.stations().size();
  if (n == 0) return;
  int third = app.contentW() / 3;
  if (x < third)          { _sel = (_sel - 1 + n) % n; _needClear = true; }
  else if (x > 2 * third) { _sel = (_sel + 1) % n;     _needClear = true; }
  _dirty = true;
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

  // Header: ICAO + flight-category pill.
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.setTextSize(2); g.drawString(m.icao, x0, y);
  g.setTextDatum(textdatum_t::top_right);
  g.setTextColor(catColor(m.cat), gTheme.bg);
  g.drawString(m.cat, cw - 6, y + 2);
  g.setTextDatum(textdatum_t::top_left);
  y += 22;
  g.setTextSize(1);
  auto line = [&](const String& s, Color c) { g.setTextColor(c, gTheme.bg); g.drawString(padRight(s, maxc), x0, y); y += 12; };

  line(m.name.substring(0, maxc) + "  " + (int)round(m.distNm) + "nm", gTheme.dim);
  if (m.wspd >= 0)
    line(m.wspd == 0 ? String("wind calm")
         : String("wind ") + (m.wdir < 0 ? String("VRB") : String(m.wdir)) + "\xF7 @ " + m.wspd + " kt", gTheme.fg);
  if (m.visSm >= 0)   line(String("vis ") + String(m.visSm, 0) + " sm", gTheme.fg);
  line(m.ceilingFt >= 0 ? String("ceiling ") + m.ceilingFt + " ft" : String("ceiling: none"), gTheme.fg);
  if (m.tempC > -999) line(String("temp ") + m.tempC + "  dewpt " + m.dewpC + " C", gTheme.fg);
  if (m.altimHpa > 0) line(String("QNH ") + m.altimHpa + " hPa", gTheme.fg);
  if (m.wx.length())  line(String("wx ") + m.wx, gTheme.warn);
  line(String(_sel + 1) + "/" + list.size() + "  (tap edges)", gTheme.dim);

  y += 3;
  g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("METAR", x0, y); y += 11;
  y = drawWrapped(g, m.raw, x0, y, maxc, 3, gTheme.fg);
  if (m.taf.length() && y < cy0 + ch - 30) {
    y += 2; g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("TAF", x0, y); y += 11;
    int maxLines = (cy0 + ch - y) / 11;
    drawWrapped(g, m.taf, x0, y, maxc, maxLines, gTheme.dim);
  }
}
