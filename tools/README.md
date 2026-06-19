# Data generators

Scripts that turn public real-world datasets into the device's bundled data. Both
re-download their sources, so re-running refreshes the data. Run from the repo root.

Python: `C:/Users/James/.platformio/penv/Scripts/python.exe` (or any Python 3).

---

## Airports + radio frequencies — `gen_airports.py`

Builds the Aircraft tab's "nearest airport + likely frequencies" table.

- **Source:** [OurAirports](https://ourairports.com) (public-domain, aggregates the
  FAA NASR data for US fields) — `airports.csv` + `airport-frequencies.csv`,
  auto-downloaded to the temp dir.
- **Output:** `data/airports.bin` — a packed binary (~80 KB) that lives in **LittleFS**
  on the device (not flash). 3877 US airports, up to 10 freqs each (TWR/GND/ATIS/CLR/
  CTAF/UNI/APP/DEP/A-D/AWOS...), kept by type priority. Format is mirrored by
  `src/services/AirportDB.cpp` — if you change the layout, change both.

### Refresh workflow (no firmware rebuild needed)

The data file updates over WiFi **without wiping LittleFS** — settings, watchlists and
touch calibration (also in LittleFS) survive. `uploadfs` would erase them, so use the
`POST /api/fs` endpoint instead:

```sh
# 1. regenerate
python tools/gen_airports.py data/airports.bin

# 2. push to the device  (MUST be octet-stream: curl's default form content-type is
#    swallowed by AsyncWebServer as params and never reaches the file handler.
#    The API is Basic-auth gated -> pass the OTA creds with -u, default admin:overhead)
curl -u admin:overhead -H "Content-Type: application/octet-stream" \
     --data-binary @data/airports.bin \
     "http://192.168.86.92/api/fs?path=/airports.bin"
#    -> {"ok":true}

# 3. reboot so AirportDB re-reads the file (it loads once at boot)
#    via the Health page two-tap reboot, power-cycle, or:
C:/Users/James/.platformio/penv/Scripts/python.exe -m esptool --port COM5 \
     --before default_reset --after hard_reset flash_id
```

Confirm on the serial console: `[apt] airport DB loaded` (vs `missing`).

> Authoritative alternative: swap the source for the FAA 28-day NASR
> `APT_BASE.csv` / `FRQ.csv` and keep the same packed layout in the emit step.

---

## World map outlines — `gen_worldmap.py`

Builds the coastline + country/state borders used by the Satellites ground track,
Launches map and Aviation pressure map.

- **Source:** [Natural Earth](https://www.naturalearthdata.com) via
  [martynafford/natural-earth-geojson](https://github.com/martynafford/natural-earth-geojson)
  — 110m countries + 50m admin-1 (US/CA/AU/BR), auto-downloaded.
- **Output:** `src/assets/Coastline.h` — `kCoastline[]` (world) + `kStateLines[]`
  (provinces), Douglas-Peucker simplified, lon/lat in 0.1-degree units. Lives in flash
  `.rodata`, so refreshing it needs a **firmware rebuild + flash** (not `/api/fs`):

```sh
python tools/gen_worldmap.py src/assets/Coastline.h
pio run -e cyd28_ili9341            # then flash via scripts/ota.ps1
```

---

## Stars + constellations — `gen_stars.py`

Builds the Star Map's catalogue (also feeds the Solar System sky-dome overlay and the
Agenda's "what's up tonight" line).

- **Sources** (all auto-downloaded, real datasets):
  - **Stars:** [HYG database](https://github.com/astronexus/HYG-Database) `hygdata_v41.csv`
    — magnitude, RA/Dec, proper names.
  - **Constellation figures:** [d3-celestial](https://github.com/ofrohn/d3-celestial)
    `constellations.lines.json` — RA/Dec polylines for all 88 figures (no star-index
    mapping, so no fragile HIP lookups).
  - **Constellation labels:** d3-celestial `constellations.json` — name + label centre.
- **Output:** `src/assets/StarCatalog.h` — `kStars[]` (name, raHours, decDeg, mag) +
  `kStarMaxMag`; `kConLines[]` figure polylines (`kSkyBreak` raHours sentinel = pen-up
  between segments) + `kConLineCount`; `kCons[]` (name + label centre) + `kConCount`;
  `kDeepSky[]` a curated set of naked-eye/binocular Messier highlights. Lives in flash
  `.rodata`, so refreshing needs a **firmware rebuild + flash**.

```sh
python tools/gen_stars.py [magLimit]   # default magLimit 5.2, capped to MAX_STARS=1500 brightest
pio run -e cyd28_ili9341               # then flash via scripts/ota.ps1
```

Knobs at the top of the script: `MAG_LIMIT` (faintest star kept), `MAX_STARS` (flash
budget — the field is ~98% of flash at 1500; lower it if you add other features),
`NAME_MAG` (only stars brighter than this carry a proper-name label). The wide Star Map
view filters to the bottom-left **mag** badge; the renderer reveals the fainter tail up
to `kStarMaxMag` as you zoom in, so a deeper catalogue costs almost nothing in the wide
view (faint stars are skipped before they're ever projected).
