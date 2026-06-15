#pragma once
#include <stdint.h>

// assets/StarCatalog — a compact bright-star catalog (J2000) + major
// constellation lines, in flash (.rodata). A measured PROTOTYPE per spec §6
// Star Map (stretch): the ~40 brightest naked-eye stars + a few constellations,
// enough to be recognisable on a no-PSRAM board. Expand later from a
// PC-generated dataset bundled in LittleFS (see BACKLOG).
struct Star {
  const char* name;
  float raHours;   // J2000 right ascension (hours)
  float decDeg;    // J2000 declination (deg)
  float mag;       // visual magnitude
};

static const Star kStars[] = {
  {"Sirius",      6.752f, -16.716f, -1.46f}, {"Canopus",   6.399f, -52.696f, -0.74f},
  {"Arcturus",   14.261f,  19.182f, -0.05f}, {"Vega",     18.616f,  38.784f,  0.03f},
  {"Capella",     5.278f,  45.998f,  0.08f}, {"Rigel",     5.242f,  -8.202f,  0.13f},
  {"Procyon",     7.655f,   5.225f,  0.34f}, {"Betelgeuse",5.919f,   7.407f,  0.50f},
  {"Achernar",    1.629f, -57.237f,  0.46f}, {"Hadar",    14.064f, -60.373f,  0.61f},
  {"Altair",     19.846f,   8.868f,  0.77f}, {"Acrux",    12.443f, -63.099f,  0.77f},
  {"Aldebaran",   4.599f,  16.509f,  0.85f}, {"Antares",  16.490f, -26.432f,  0.96f},
  {"Spica",      13.420f, -11.161f,  0.98f}, {"Pollux",    7.755f,  28.026f,  1.14f},
  {"Fomalhaut",  22.961f, -29.622f,  1.16f}, {"Deneb",    20.690f,  45.280f,  1.25f},
  {"Mimosa",     12.795f, -59.689f,  1.25f}, {"Regulus",  10.139f,  11.967f,  1.35f},
  {"Adhara",      6.977f, -28.972f,  1.50f}, {"Castor",    7.577f,  31.888f,  1.58f},
  {"Shaula",     17.560f, -37.104f,  1.62f}, {"Bellatrix", 5.418f,   6.350f,  1.64f},
  {"Elnath",      5.438f,  28.608f,  1.65f}, {"Alnilam",   5.604f,  -1.202f,  1.69f},
  {"Alnitak",     5.679f,  -1.943f,  1.74f}, {"Mintaka",   5.533f,  -0.299f,  2.23f},
  {"Saiph",       5.796f,  -9.670f,  2.06f}, {"Dubhe",    11.062f,  61.751f,  1.79f},
  {"Merak",      11.031f,  56.382f,  2.37f}, {"Phecda",   11.897f,  53.695f,  2.44f},
  {"Megrez",     12.257f,  57.033f,  3.31f}, {"Alioth",   12.900f,  55.960f,  1.77f},
  {"Mizar",      13.399f,  54.925f,  2.04f}, {"Alkaid",   13.792f,  49.313f,  1.86f},
  {"Polaris",     2.530f,  89.264f,  1.98f}, {"Alpheratz", 0.140f,  29.090f,  2.06f},
  {"Schedar",     0.675f,  56.537f,  2.24f}, {"Caph",      0.153f,  59.150f,  2.28f},
};
static const int kStarCount = sizeof(kStars) / sizeof(kStars[0]);

// Constellation lines as star-name pairs (resolved at draw).
struct StarLine { const char* a; const char* b; };
static const StarLine kStarLines[] = {
  // Orion
  {"Betelgeuse","Bellatrix"}, {"Bellatrix","Mintaka"}, {"Betelgeuse","Alnitak"},
  {"Mintaka","Alnilam"}, {"Alnilam","Alnitak"}, {"Mintaka","Rigel"},
  {"Alnitak","Saiph"}, {"Rigel","Saiph"},
  // Big Dipper (Ursa Major)
  {"Dubhe","Merak"}, {"Merak","Phecda"}, {"Phecda","Megrez"}, {"Megrez","Dubhe"},
  {"Megrez","Alioth"}, {"Alioth","Mizar"}, {"Mizar","Alkaid"},
  // Cassiopeia
  {"Caph","Schedar"},
};
static const int kStarLineCount = sizeof(kStarLines) / sizeof(kStarLines[0]);
