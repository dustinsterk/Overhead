#pragma once
#include <stdint.h>

// assets/Coastline — a very coarse world coastline as lon/lat polylines, for
// orienting the satellite ground track (spec §6). Lives in flash (.rodata).
//
// This is a low-res hand-built outline, NOT a real dataset — recognisable, not
// accurate. A proper Natural Earth coastline bundled in LittleFS (data/) is the
// intended upgrade. {999,999} is a pen-up separator between polylines.
struct CoastPt { int16_t lon; int16_t lat; };

static const CoastPt kCoastline[] = {
  // North America
  {-165,60},{-150,70},{-125,70},{-100,69},{-85,67},{-65,60},{-55,52},{-65,45},
  {-75,35},{-80,26},{-97,26},{-105,22},{-110,30},{-120,34},{-125,48},{-135,58},
  {-150,59},{-165,60},
  {999,999},
  // Greenland
  {-45,60},{-20,70},{-20,83},{-60,83},{-55,76},{-50,68},{-45,60},
  {999,999},
  // South America
  {-78,8},{-60,5},{-50,0},{-35,-8},{-40,-22},{-48,-28},{-58,-38},{-65,-45},
  {-68,-52},{-73,-50},{-72,-40},{-70,-20},{-75,-14},{-81,-5},{-79,2},{-78,8},
  {999,999},
  // Africa
  {-17,15},{-10,5},{5,4},{10,0},{13,-8},{15,-30},{20,-35},{32,-26},{40,-15},
  {51,12},{43,12},{33,30},{10,33},{-10,30},{-17,15},
  {999,999},
  // Eurasia
  {-9,39},{-5,48},{5,53},{10,57},{30,70},{60,70},{100,76},{140,73},{170,68},
  {160,60},{140,53},{135,45},{122,40},{121,31},{110,21},{105,10},{100,8},
  {97,16},{90,22},{80,8},{70,20},{62,25},{57,25},{48,30},{35,37},{28,40},
  {20,40},{12,44},{3,42},{-9,39},
  {999,999},
  // Australia
  {114,-22},{122,-18},{130,-12},{142,-11},{150,-25},{153,-28},{150,-37},
  {140,-38},{129,-32},{115,-34},{114,-22},
};

static const int kCoastlineCount = sizeof(kCoastline) / sizeof(kCoastline[0]);
