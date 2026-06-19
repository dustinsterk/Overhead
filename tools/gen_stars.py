#!/usr/bin/env python3
"""gen_stars.py — bake the star + constellation database into src/assets/StarCatalog.h.

Real catalogs, downloaded fresh each run (same idea as gen_worldmap.py — a flash
header in .rodata, no device RAM cost, updatable by re-running + reflash):

  * Stars      : HYG database (astronexus) -> kStars[] (name, raHours, decDeg, mag),
                 filtered to a magnitude limit. Proper names kept only for the
                 brighter stars (label budget); fainter stars are unnamed dots.
  * Con. lines : d3-celestial constellations.lines.json -> kConLines[] as RA/Dec
                 polyline vertices (RA in hours, Dec in deg), with a {SKY_BREAK} pen-up
                 sentinel between segments. Drawn directly (no star-index mapping).
  * Con. labels: d3-celestial constellations.json -> kCons[] (name + label centre).

Usage:  python tools/gen_stars.py [magLimit]      (default 5.0)
Requires internet. Writes src/assets/StarCatalog.h.
"""
import sys, os, io, csv, json, urllib.request

MAG_LIMIT  = float(sys.argv[1]) if len(sys.argv) > 1 else 5.2
NAME_MAG   = 3.2          # keep proper names only for stars at least this bright
MAX_STARS  = 1500         # safety cap. Wide view filters to bright stars (cheap: the
                          # render skips faint ones before projecting); the fainter tail
                          # is only revealed/projected when zoomed in.
HYG_URL    = "https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv"
LINES_URL  = "https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/constellations.lines.json"
CONS_URL   = "https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/constellations.json"
OUT        = os.path.join(os.path.dirname(__file__), "..", "src", "assets", "StarCatalog.h")
SKY_BREAK  = 99.0         # raHours sentinel = pen up between polyline segments

# Curated naked-eye / binocular deep-sky highlights (name, RA hours, Dec deg, J2000).
# Real Messier/NGC coordinates — a short hand-picked set, not the full catalog.
DEEPSKY = [
    ("M31 Andromeda",   0.712,  41.27),
    ("M45 Pleiades",    3.790,  24.11),
    ("M42 Orion Neb",   5.588,  -5.39),
    ("M44 Beehive",     8.670,  19.67),
    ("M13 Hercules",   16.695,  36.46),
    ("M8 Lagoon",      18.060, -24.38),
    ("M22 Sagittarius",18.607, -23.90),
    ("M7 Ptolemy",     17.897, -34.79),
    ("M24 Star Cloud", 18.282, -18.55),
    ("M57 Ring Neb",   18.893,  33.03),
    ("M27 Dumbbell",   19.994,  22.72),
    ("Double Cluster",  2.333,  57.13),
    ("M81 Bode",        9.926,  69.07),
    ("M51 Whirlpool",  13.498,  47.20),
    ("Omega Cen",      13.447, -47.48),
    ("47 Tucanae",      0.401, -72.08),
    ("LMC",             5.392, -69.76),
    ("SMC",             0.877, -72.83),
]

def fetch(url, binary=False):
    print("  GET", url)
    with urllib.request.urlopen(url, timeout=120) as r:
        d = r.read()
    return d if binary else d.decode("utf-8", "replace")

def cesc(s):  # C string-literal escape
    return s.replace("\\", "\\\\").replace('"', '\\"')

def main():
    print("[stars] downloading HYG...")
    rows = list(csv.DictReader(io.StringIO(fetch(HYG_URL))))
    stars = []
    for r in rows:
        try:
            mag = float(r["mag"]); ra = float(r["ra"]); dec = float(r["dec"])
        except (ValueError, KeyError):
            continue
        if mag > MAG_LIMIT:
            continue
        name = (r.get("proper") or "").strip()
        if mag > NAME_MAG:
            name = ""                       # too faint to label -> just a dot
        stars.append((mag, ra, dec, name))
    stars.sort(key=lambda s: s[0])          # brightest first (labels favour them)
    if len(stars) > MAX_STARS:
        stars = stars[:MAX_STARS]
    named = sum(1 for s in stars if s[3])
    print(f"[stars] {len(stars)} stars (mag<= {MAG_LIMIT}), {named} named")

    print("[stars] downloading constellation lines...")
    lines = json.loads(fetch(LINES_URL))
    verts = []                              # (raHours, decDeg) with SKY_BREAK pen-ups
    for f in lines["features"]:
        for seg in f["geometry"]["coordinates"]:
            if not seg:
                continue
            for raDeg, dec in seg:
                ra = (raDeg % 360.0) / 15.0  # deg(-180..180) -> hours 0..24
                verts.append((ra, dec))
            verts.append((SKY_BREAK, 0.0))   # pen up
    print(f"[stars] {len(verts)} line vertices")

    print("[stars] downloading constellation labels...")
    cons = json.loads(fetch(CONS_URL))
    labels = []
    for f in cons["features"]:
        nm = (f["properties"].get("name") or f.get("id") or "").strip()
        g = f.get("geometry") or {}
        c = g.get("coordinates")
        if not nm or not c:
            continue
        ra = (float(c[0]) % 360.0) / 15.0
        labels.append((nm, ra, float(c[1])))
    labels.sort(key=lambda x: x[0])
    print(f"[stars] {len(labels)} constellations")

    with open(os.path.abspath(OUT), "w", encoding="utf-8") as o:
        o.write("#pragma once\n#include <stdint.h>\n\n")
        o.write("// assets/StarCatalog - GENERATED by tools/gen_stars.py. Do not edit by hand.\n")
        o.write(f"// Stars: HYG database, mag <= {MAG_LIMIT} ({len(stars)} stars, {named} named).\n")
        o.write("// Constellation figures: d3-celestial (RA/Dec polylines). J2000.\n\n")
        o.write("struct Star { const char* name; float raHours; float decDeg; float mag; };\n")
        o.write(f"static const Star kStars[] = {{\n")
        for mag, ra, dec, name in stars:
            o.write(f'  {{"{cesc(name)}", {ra:.4f}f, {dec:.3f}f, {mag:.2f}f}},\n')
        o.write("};\n")
        o.write("static const int kStarCount = sizeof(kStars) / sizeof(kStars[0]);\n")
        faint = max((s[0] for s in stars), default=MAG_LIMIT)
        o.write(f"static constexpr float kStarMaxMag = {faint:.2f}f;  // faintest star in the catalogue\n\n")

        o.write("// Constellation figure polylines: raHours==kSkyBreak is a pen-up between segments.\n")
        o.write(f"static constexpr float kSkyBreak = {SKY_BREAK:.1f}f;\n")
        o.write("struct SkyVtx { float raHours; float decDeg; };\n")
        o.write("static const SkyVtx kConLines[] = {\n")
        for ra, dec in verts:
            o.write(f"  {{{ra:.4f}f, {dec:.3f}f}},\n")
        o.write("};\n")
        o.write("static const int kConLineCount = sizeof(kConLines) / sizeof(kConLines[0]);\n\n")

        o.write("// Constellations: display name + label centre (raHours, decDeg).\n")
        o.write("struct Constellation { const char* name; float raHours; float decDeg; };\n")
        o.write("static const Constellation kCons[] = {\n")
        for nm, ra, dec in labels:
            o.write(f'  {{"{cesc(nm)}", {ra:.4f}f, {dec:.3f}f}},\n')
        o.write("};\n")
        o.write("static const int kConCount = sizeof(kCons) / sizeof(kCons[0]);\n\n")

        o.write("// Curated deep-sky highlights (naked-eye / binocular). Real coordinates, J2000.\n")
        o.write("struct DeepSky { const char* name; float raHours; float decDeg; };\n")
        o.write("static const DeepSky kDeepSky[] = {\n")
        for nm, ra, dec in DEEPSKY:
            o.write(f'  {{"{cesc(nm)}", {ra:.4f}f, {dec:.3f}f}},\n')
        o.write("};\n")
        o.write("static const int kDeepSkyCount = sizeof(kDeepSky) / sizeof(kDeepSky[0]);\n")
    print(f"[stars] wrote {os.path.abspath(OUT)}  ({len(DEEPSKY)} deep-sky)")

if __name__ == "__main__":
    main()
