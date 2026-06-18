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
  // --- expansion: stars needed for more constellation figures (J2000) ---
  {"Navi",        0.945f,  60.717f,  2.47f}, {"Ruchbah",   1.430f,  60.235f,  2.68f},
  {"Segin",       1.906f,  63.670f,  3.35f}, {"Sadr",     20.370f,  40.257f,  2.23f},
  {"Gienah",     20.770f,  33.970f,  2.48f}, {"Fawaris",  19.750f,  45.131f,  2.87f},
  {"Albireo",    19.512f,  27.960f,  3.18f}, {"Sheliak",  18.835f,  33.363f,  3.52f},
  {"Sulafat",    18.982f,  32.690f,  3.26f}, {"Denebola", 11.818f,  14.572f,  2.11f},
  {"Algieba",    10.333f,  19.842f,  2.21f}, {"Zosma",    11.235f,  20.524f,  2.56f},
  {"Chertan",    11.237f,  15.430f,  3.33f}, {"Alhena",    6.628f,  16.399f,  1.93f},
  {"Wezen",       7.140f, -26.393f,  1.83f}, {"Mirzam",    6.378f, -17.956f,  1.98f},
  {"Gacrux",     12.519f, -57.113f,  1.63f}, {"Imai",     12.252f, -58.749f,  2.79f},
  {"Kochab",     14.845f,  74.156f,  2.07f}, {"Pherkad",  15.345f,  71.834f,  3.00f},
  {"Tarazed",    19.771f,  10.613f,  2.72f}, {"Alshain",  19.922f,   6.407f,  3.71f},
  {"Izar",       14.750f,  27.074f,  2.35f}, {"Seginus",  14.535f,  38.308f,  3.03f},
  {"Nekkar",     15.032f,  40.390f,  3.49f}, {"Muphrid",  13.912f,  18.398f,  2.68f},
  {"Markab",     23.079f,  15.205f,  2.48f}, {"Scheat",   23.063f,  28.083f,  2.42f},
  {"Algenib",     0.221f,  15.184f,  2.83f}, {"Mirach",    1.162f,  35.621f,  2.05f},
  {"Almach",      2.065f,  42.330f,  2.10f}, {"Mirfak",    3.405f,  49.861f,  1.79f},
  {"Algol",       3.136f,  40.956f,  2.12f}, {"Menkalinan",5.992f,  44.947f,  1.90f},
  {"Mahasim",     5.995f,  37.213f,  2.62f}, {"Gomeisa",   7.453f,   8.289f,  2.89f},
  {"Dschubba",   16.005f, -22.622f,  2.32f}, {"Acrab",    16.090f, -19.805f,  2.62f},
  {"Sargas",     17.622f, -42.998f,  1.86f}, {"Lesath",   17.513f, -37.296f,  2.70f},
};
static const int kStarCount = sizeof(kStars) / sizeof(kStars[0]);

// Prominent deep-sky (Messier) objects — naked-eye / binocular showpieces, J2000.
// Plotted as distinct markers on the Star Map. Coordinates are accurate; this is a
// curated highlights list, not the full Messier catalog.
struct DeepSky { const char* name; float raHours; float decDeg; };
static const DeepSky kDeepSky[] = {
  {"M31 Andromeda", 0.712f,  41.269f},
  {"M45 Pleiades",  3.790f,  24.105f},
  {"M42 Orion Neb", 5.588f,  -5.391f},
  {"M44 Beehive",   8.670f,  19.667f},
  {"M13 Hercules", 16.695f,  36.460f},
  {"M6 Butterfly", 17.668f, -32.217f},
  {"M7 Ptolemy",   17.897f, -34.793f},
  {"M8 Lagoon",    18.060f, -24.383f},
  {"M22 Sagit",    18.606f, -23.904f},
  {"M11 Wild Duck",18.851f,  -6.270f},
  {"M57 Ring",     18.885f,  33.029f},
  {"M27 Dumbbell", 19.994f,  22.721f},
};
static const int kDeepSkyCount = sizeof(kDeepSky) / sizeof(kDeepSky[0]);

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
  // Cassiopeia (the W)
  {"Caph","Schedar"}, {"Schedar","Navi"}, {"Navi","Ruchbah"}, {"Ruchbah","Segin"},
  // Cygnus (Northern Cross)
  {"Deneb","Sadr"}, {"Sadr","Gienah"}, {"Sadr","Fawaris"}, {"Sadr","Albireo"},
  // Lyra
  {"Vega","Sheliak"}, {"Vega","Sulafat"}, {"Sheliak","Sulafat"},
  // Leo (sickle + hindquarters triangle)
  {"Regulus","Algieba"}, {"Algieba","Zosma"}, {"Zosma","Denebola"},
  {"Denebola","Chertan"}, {"Chertan","Regulus"},
  // Gemini
  {"Castor","Pollux"}, {"Pollux","Alhena"},
  // Canis Major
  {"Mirzam","Sirius"}, {"Sirius","Wezen"}, {"Wezen","Adhara"}, {"Adhara","Sirius"},
  // Crux (Southern Cross)
  {"Acrux","Gacrux"}, {"Mimosa","Imai"},
  // Ursa Minor (Little Dipper bowl edge)
  {"Kochab","Pherkad"},
  // Aquila
  {"Tarazed","Altair"}, {"Altair","Alshain"},
  // Bootes (the kite)
  {"Arcturus","Izar"}, {"Izar","Nekkar"}, {"Nekkar","Seginus"},
  {"Seginus","Arcturus"}, {"Arcturus","Muphrid"},
  // Pegasus (Great Square) + Andromeda chain + Perseus
  {"Alpheratz","Scheat"}, {"Scheat","Markab"}, {"Markab","Algenib"}, {"Algenib","Alpheratz"},
  {"Alpheratz","Mirach"}, {"Mirach","Almach"}, {"Almach","Mirfak"}, {"Mirfak","Algol"},
  // Auriga (pentagon, shares Elnath with Taurus)
  {"Capella","Menkalinan"}, {"Menkalinan","Mahasim"}, {"Mahasim","Elnath"}, {"Elnath","Capella"},
  // Taurus horn + Canis Minor
  {"Aldebaran","Elnath"}, {"Procyon","Gomeisa"},
  // Scorpius (head + hook)
  {"Acrab","Dschubba"}, {"Dschubba","Antares"}, {"Antares","Sargas"},
  {"Sargas","Shaula"}, {"Shaula","Lesath"},
};
static const int kStarLineCount = sizeof(kStarLines) / sizeof(kStarLines[0]);

// Constellations: name + up to 8 member stars (must match catalog names). Shared
// by the Star Map (zoom-tour framing + labels) and the Agenda (what's up tonight).
struct Constellation { const char* name; const char* stars[8]; };
static const Constellation kCons[] = {
  {"Orion",       {"Betelgeuse","Bellatrix","Mintaka","Alnilam","Alnitak","Rigel","Saiph"}},
  {"Ursa Major",  {"Dubhe","Merak","Phecda","Megrez","Alioth","Mizar","Alkaid"}},
  {"Cassiopeia",  {"Caph","Schedar","Navi","Ruchbah","Segin"}},
  {"Cygnus",      {"Deneb","Sadr","Gienah","Fawaris","Albireo"}},
  {"Lyra",        {"Vega","Sheliak","Sulafat"}},
  {"Leo",         {"Regulus","Algieba","Zosma","Denebola","Chertan"}},
  {"Bootes",      {"Arcturus","Izar","Seginus","Nekkar","Muphrid"}},
  {"Scorpius",    {"Antares","Dschubba","Acrab","Shaula","Sargas","Lesath"}},
  {"Gemini",      {"Castor","Pollux","Alhena"}},
  {"Canis Major", {"Sirius","Mirzam","Wezen","Adhara"}},
  {"Crux",        {"Acrux","Mimosa","Gacrux","Imai"}},
  {"Pegasus",     {"Markab","Scheat","Algenib","Alpheratz"}},
  {"Aquila",      {"Altair","Tarazed","Alshain"}},
  {"Auriga",      {"Capella","Menkalinan","Mahasim","Elnath"}},
};
static const int kConCount = sizeof(kCons) / sizeof(kCons[0]);
