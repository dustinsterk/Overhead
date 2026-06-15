# Polish backlog

Deferred polish per milestone — intentionally cut from the first pass to keep
momentum. Pick up later. (Bugs/blocking work go in commits/tasks, not here.)

## M0 — bring-up / HAL
- Verify 4" CYD (ST7796) + CrowPanel panels on real hardware (only 2.8" CYD verified).
- CrowPanel: confirm RGB porch timings, I2C-expander backlight, GT911 coexistence on hw.
- DS3231/PCF8563 RTC driver (currently a stub; NTP-only for now).

## M1 — services / infra
- core/Canvas + Renderer abstraction + reusable widget toolkit (pages still draw via Display::gfx()).
- Full app-shell chrome: 3x3 quick-jump grid, corner-glyph overlays (Settings/Health/Location).
- WebPortal: richer settings UI (forms, not raw JSON), auth on all routes, OTA basic-auth verified.
- Open-Meteo timezone refinement (DST rules; currently fixed offset from IP).
- GPS location source (module optional).

## M2 — astro core
- Enable/validate Ephem (GPLv3) for full planet precision when the Solar System tab needs it.
- deltaT refinement; higher-precision sun/moon if required.
- Turn ASTRO_SELFTEST off in release builds.

## M3 — Satellites
- On-device group filter chips + sunlit-only toggle; AMSAT mode/band sub-filters.
- Source the live AMSAT transponder set (not the hand seed in Transponders.h).
- Proper Natural Earth coastline bundled in LittleFS (current is a coarse hand outline).
- Grayline / day-night terminator overlay on the ground track (needs Ephem subsolar, m6).
- Doppler: uplink correction + tuning readout; rotor/Az-El output.
- Watchlist editing on-device (currently seeded; edit via web only later).

<!-- new milestones append below as they land -->
