#include "PageMissions.h"
#include "../core/App.h"
#include "../core/Theme.h"
#include "../hal/Display.h"
#include "../services/TimeService.h"
#include "../services/LocationService.h"
#include "../providers/MarsProvider.h"
#include "../astro/SolarSystem.h"
#include <math.h>
#include <time.h>

static constexpr double D2R = 3.14159265358979323846 / 180.0;
static constexpr double SOL = 88775.244;            // Mars solar day (s)

// Landing epochs (UTC) for the active rovers.
static constexpr time_t PERSEV_LANDING = 1613681700; // 2021-02-18 20:55 UTC
static constexpr time_t CURIO_LANDING  = 1344230220; // 2012-08-06 05:17 UTC

void PageMissions::tick(App& app, uint32_t nowMs) {
  if (!_dirty && nowMs - _lastDraw < 30000) return;  // slow-changing
  _dirty = false; _lastDraw = nowMs;
  draw(app);
}

void PageMissions::onTouch(App& app, int x, int y) {
  int third = app.contentW() / 3;
  if (x >= third && x <= 2 * third) { _view = (_view + 1) % 2; _dirty = true; }  // centre cycles
}

void PageMissions::draw(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  g.fillRect(0, cy0, cw, ch, gTheme.bg);
  g.setTextDatum(textdatum_t::top_left);
  g.setTextSize(1);
  if (!_time.synced()) {
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(gTheme.dim, gTheme.bg);
    g.drawString("waiting for time sync...", cw / 2, cy0 + ch / 2);
    return;
  }
  if (_view == 0) drawMars(app); else drawDeepSpace(app);
}

void PageMissions::drawMars(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  g.setTextDatum(textdatum_t::top_left);
  g.setTextSize(1);
  double jd = _time.julianDate();
  time_t now = time(nullptr);
  int x = 6, y = cy0 + 4;

  // --- Mars, live ---
  astro::HelioPos e = astro::heliocentricBody(2, jd);   // Earth
  astro::HelioPos m = astro::heliocentricBody(3, jd);   // Mars
  double dl = (m.lonDeg - e.lonDeg) * D2R;
  double distAu = sqrt(e.rAu * e.rAu + m.rAu * m.rAu - 2 * e.rAu * m.rAu * cos(dl));
  double ltMin = distAu * 149597870.7 / 299792.458 / 60.0;

  g.setTextColor(gTheme.warn, gTheme.bg);
  g.setTextSize(2); g.drawString("Mars", x, y);
  g.setTextSize(1);
  char b[56];
  snprintf(b, sizeof(b), "%.2f AU  %.1f lt-min", distAu, ltMin);
  g.setTextColor(gTheme.fg, gTheme.bg); g.drawString(b, x + 56, y + 3); y += 18;
  if (_loc.active().valid) {
    astro::PlanetState ms = astro::planetState(astro::Planet::Mars, jd, _loc.active().lat, _loc.active().lon);
    if (ms.above) snprintf(b, sizeof(b), "in your sky now: el %d\xF7 az %d\xF7", (int)round(ms.elDeg), (int)round(ms.azDeg));
    else          snprintf(b, sizeof(b), "below your horizon now");
    g.setTextColor(ms.above ? gTheme.ok : gTheme.dim, gTheme.bg); g.drawString(b, x, y); y += 13;
  }

  // --- Mars surface map with the rover landing sites ---
  struct Rover { const char* name; const char* lbl; const char* site; const char* landed; time_t landing; float lat, lonE; const RoverInfo& info; };
  Rover rv[] = {
    { "Perseverance", "Pe", "Jezero",     "2021-02-18", PERSEV_LANDING, 18.44f,  77.45f, _mars.perseverance() },
    { "Curiosity",    "Cu", "Gale",       "2012-08-06", CURIO_LANDING,  -4.59f, 137.44f, _mars.curiosity() },
  };
  int mx = 6, mw = cw - 12, my = y + 1, mh = 84;
  g.fillRect(mx, my, mw, mh, rgb565(70, 34, 22));    // dim Mars ochre
  g.drawRect(mx, my, mw, mh, gTheme.grid);
  g.drawFastHLine(mx, my + mh / 2, mw, gTheme.dim);  // equator
  for (int lon = 90; lon < 360; lon += 90) g.drawFastVLine(mx + lon * mw / 360, my, mh, gTheme.dim);
  g.setTextColor(gTheme.dim, gTheme.bg); g.setTextDatum(textdatum_t::top_left);
  g.drawString("Mars surface", mx + 3, my + 2);
  auto FX = [&](float lonE) { return mx + (int)(lonE / 360.0f * mw); };
  auto FY = [&](float lat)  { return my + (int)((90.0f - lat) / 180.0f * mh); };

  // Major surface features (real areographic coords, planetocentric east lon).
  // kind: 0=volcano/peak (triangle), 1=basin/lowland (ring), 2=canyon (bar).
  struct Feat { const char* name; float lat, lonE; uint8_t kind; };
  static const Feat ft[] = {
    { "Olympus",   18.65f, 226.2f, 0 },   // tallest volcano in the solar system
    { "Tharsis",    0.8f,  247.0f, 0 },   // the three aligned Tharsis Montes
    { "Elysium",   25.0f,  147.0f, 0 },
    { "Marineris", -13.9f, 300.0f, 2 },   // Valles Marineris canyon system
    { "Hellas",   -42.4f,   70.5f, 1 },   // giant impact basin
  };
  for (auto& f : ft) {
    int fx = FX(f.lonE), fy = FY(f.lat);
    if (f.kind == 0)      g.fillTriangle(fx, fy - 3, fx - 3, fy + 2, fx + 3, fy + 2, gTheme.accent);
    else if (f.kind == 1) g.drawCircle(fx, fy, 3, gTheme.accent);
    else                  g.drawFastHLine(fx - 4, fy, 9, gTheme.accent);
    bool right = fx < mx + mw - 54;
    g.setTextColor(gTheme.accent, gTheme.bg);
    g.setTextDatum(right ? textdatum_t::middle_left : textdatum_t::middle_right);
    g.drawString(f.name, right ? fx + 5 : fx - 5, fy);
  }
  for (auto& r : rv) {                                 // rover sites on top
    int rx = FX(r.lonE), ry = FY(r.lat);
    g.fillCircle(rx, ry, 2, gTheme.ok);
    g.setTextColor(gTheme.fg, gTheme.bg); g.setTextDatum(textdatum_t::middle_left);
    g.drawString(r.lbl, rx + 4, ry);
  }
  y = my + mh + 3;

  // --- Rover status (compact) ---
  auto solNow = [&](time_t landing) { return now > landing ? (long)((now - landing) / SOL) : -1L; };
  g.setTextDatum(textdatum_t::top_left);
  for (auto& r : rv) {
    long sol = solNow(r.landing);
    g.setTextColor(gTheme.accent, gTheme.bg); g.drawString(r.name, x, y);
    if (r.info.maxSol >= 0) {
      bool act = r.info.status.equalsIgnoreCase("active");
      g.setTextColor(act ? gTheme.ok : gTheme.dim, gTheme.bg);
      snprintf(b, sizeof(b), "sol %ld %s  last %s", sol, act ? "ACTIVE" : "done", r.info.maxDate.c_str());
    } else {
      g.setTextColor(gTheme.dim, gTheme.bg);
      snprintf(b, sizeof(b), "sol %ld  %s  %s", sol, r.site, _mars.status() == ProviderStatus::Error ? "(no feed)" : "");
    }
    g.drawString(b, x + 78, y); y += 13;
  }
}

// Iconic active deep-space missions. Distance for the receding probes is
// extrapolated from a reference epoch + recession rate (good to ~1 AU); en-route /
// stationed missions show their target instead. Light-time turns "far" into "wow".
void PageMissions::drawDeepSpace(App& app) {
  auto& g = app.display().gfx();
  const int cw = app.contentW(), cy0 = app.contentY(), ch = app.contentH();
  float yr = 2000.0f + (float)((_time.julianDate() - 2451545.0) / 365.25);

  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(gTheme.accent, gTheme.bg);
  g.setTextSize(2); g.drawString("Deep Space", 6, cy0 + 2); g.setTextSize(1);
  g.setTextColor(gTheme.dim, gTheme.bg); g.drawString("[tap mid: Mars]", 150, cy0 + 9);
  int y = cy0 + 24;

  struct DS { const char* name; const char* note; float au; float rate; float baseYr; };
  static const DS d[] = {
    {"Voyager 1",   "interstellar",         165.6f, 3.58f, 2025.5f},
    {"Voyager 2",   "interstellar",         138.2f, 3.27f, 2025.5f},
    {"New Horizons","Kuiper Belt",           60.5f, 2.95f, 2025.5f},
    {"Psyche",      "metal asteroid '29",     0,    0,     0},
    {"Eur.Clipper", "Jupiter/Europa '30",     0,    0,     0},
    {"JWST",        "Sun-Earth L2 1.5Mkm",    0,    0,     0},
    {"Parker SP",   "skims the Sun",          0,    0,     0},
  };
  char b[56];
  for (auto& m : d) {
    if (y > cy0 + ch - 12) break;
    g.setTextDatum(textdatum_t::top_left);
    g.setTextColor(gTheme.fg, gTheme.bg);  g.drawString(m.name, 6, y);
    if (m.au > 0) {
      float dist = m.au + m.rate * (yr - m.baseYr);
      snprintf(b, sizeof(b), "%d AU  %.0f lt-hr  %s", (int)dist, dist * 8.317f / 60.0f, m.note);
    } else {
      snprintf(b, sizeof(b), "-> %s", m.note);
    }
    g.setTextColor(gTheme.dim, gTheme.bg);  g.drawString(b, 92, y);
    y += 13;
  }
}
