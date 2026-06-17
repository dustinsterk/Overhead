#pragma once
#include <Arduino.h>
#include <time.h>

// assets/MeteorShowers — the major annual meteor showers (fixed-date peaks +
// active windows + nominal ZHR). Surfaced on the Agenda as the active shower or a
// countdown to the next one. Peak dates drift only ~a day year-to-year, so a
// static table is fine for a dashboard.
struct MeteorShower {
  const char* name;
  uint8_t  pkM, pkD;      // peak month/day
  uint8_t  stM, stD;      // active-window start
  uint8_t  enM, enD;      // active-window end
  uint16_t zhr;           // nominal zenithal hourly rate
};
static const MeteorShower kShowers[] = {
  {"Quadrantids",     1, 3,  12,28,  1,12, 120},
  {"Lyrids",          4,22,  4,16,   4,25,  18},
  {"Eta Aquariids",   5, 6,  4,19,   5,28,  50},
  {"Delta Aquariids", 7,30,  7,12,   8,23,  25},
  {"Perseids",        8,12,  7,17,   8,24, 100},
  {"Draconids",      10, 8, 10, 6,  10,10,  10},
  {"Orionids",       10,21, 10, 2,  11, 7,  20},
  {"Leonids",        11,17, 11, 6,  11,30,  15},
  {"Geminids",       12,14, 12, 4,  12,17, 150},
  {"Ursids",         12,22, 12,17,  12,26,  10},
};
static const int kShowerCount = sizeof(kShowers) / sizeof(kShowers[0]);

static inline int meteorDOY(int m, int d) {
  static const int cum[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  return cum[(m - 1) % 12] + d;     // 1..365 (non-leap; ~1 day slop is fine)
}

struct ShowerInfo { const char* name; int zhr; int daysToPeak; bool active; };

// The most relevant shower for `now`: the active one (highest ZHR if several), else
// the nearest upcoming peak. daysToPeak is signed (negative = peak just passed).
static inline ShowerInfo meteorShowerInfo(time_t now) {
  struct tm tm; localtime_r(&now, &tm);
  int doy = tm.tm_yday + 1;
  int best = -1;
  for (int i = 0; i < kShowerCount; ++i) {
    int s = meteorDOY(kShowers[i].stM, kShowers[i].stD);
    int e = meteorDOY(kShowers[i].enM, kShowers[i].enD);
    bool act = (s <= e) ? (doy >= s && doy <= e) : (doy >= s || doy <= e);   // Dec->Jan wrap
    if (act && (best < 0 || kShowers[i].zhr > kShowers[best].zhr)) best = i;
  }
  if (best >= 0) {
    int dtp = meteorDOY(kShowers[best].pkM, kShowers[best].pkD) - doy;
    if (dtp < -300) dtp += 365; else if (dtp > 300) dtp -= 365;
    return { kShowers[best].name, kShowers[best].zhr, dtp, true };
  }
  int bestd = 9999;
  for (int i = 0; i < kShowerCount; ++i) {
    int d = (meteorDOY(kShowers[i].pkM, kShowers[i].pkD) - doy + 365) % 365;
    if (d < bestd) { bestd = d; best = i; }
  }
  return { kShowers[best].name, kShowers[best].zhr, bestd, false };
}
