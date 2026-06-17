# PIO_DEBUG.md — autonomous build / flash / see-the-screen / drive-the-device loop

This document is written **for an AI coding agent** (or a human) who wants to work on
this ESP32 firmware *without a human in the loop holding the board* — edit code, flash
it over WiFi, look at the actual screen, tap/swipe the UI, read status, verify, commit.

It explains how the loop was built on this project (Overhead, a CYD dashboard) so it can
be **replicated on another ESP32 + display project**. The pieces are independent; take
what you need.

---

## 0. TL;DR — the loop

```
edit → build (PlatformIO) → OTA flash (WiFi) → screenshot (JPEG over HTTP)
     → tap/swipe (HTTP) → read /api/status → eyeball the .jpg → commit
```

Two host commands do the I/O, and both are stable single-line invocations so they can be
**allow-listed once** (no per-call approval):

- Flash:      `pwsh -NoProfile -File scripts/ota.ps1`
- Screenshot: `pwsh -NoProfile -File scripts/shot.ps1`  → writes `.pio/shot.jpg`

Everything else is plain `curl` to the device's HTTP API.

---

## 1. Toolchain (PlatformIO)

PlatformIO is **not on PATH** in this environment. Invoke the venv binary directly:

```
C:/Users/James/.platformio/penv/Scripts/platformio.exe run -e <env>
```

Environments (`platformio.ini`):
- `cyd28_ili9341` — the verified board (2.8" ESP32-2432S028R, ILI9341, **no PSRAM**), classic `espressif32`.
- `cyd4_st7796`   — 4" ST7796 variant.
- `crowpanel_s3_5hmi` — ESP32-S3 parallel-RGB, uses the **pioarduino** platform (arduino-esp32 3.x) because the S3 `esp_lcd` RGB driver LovyanGFX needs isn't in the classic 2.0.x platform.

**pioarduino bootstrap glitch (S3 env only):** install reports "Failed to install Python
dependencies" and the build dies with `No module named 'esptool'`. Fix once:
```
C:/Users/James/.platformio/penv/Scripts/python.exe -m pip install esptool==5.3.0
```

A green build prints a `RAM: … Flash: …` size table and `[SUCCESS]`.

---

## 2. On-device: the screenshot endpoint (the hard part)

The screen is an SPI TFT driven by **LovyanGFX**. To "see" it remotely we read the panel
back, JPEG-encode it on the device, and serve it over HTTP. Files: `src/hal/Display.cpp/.h`.

Key decisions (all forced by **no PSRAM** — see §6):

1. **Encode JPEG, not BMP.** A full BMP is ~150 KB; the heap can't hold it. JPEG via
   [bitbank2 **JPEGENC**] fits a 320×240 frame in ~8–14 KB.
2. **One boot-time buffer.** `_jpg` (16 KB) is `malloc`'d **once in `begin()`** and never
   freed. A runtime malloc/free of that size fragments the heap and drops the largest free
   block below the TLS floor (§6), which starves the HTTPS providers. Allocate at boot on
   the fresh heap and keep it.
3. **Encode on the UI thread, MCU-by-MCU.** `Display::serviceShot()` reads the panel in
   16×16 blocks with `gfx().readRect(...)` and feeds each to the encoder. Doing it on the
   UI/loop task (not the AsyncTCP task) keeps the single AsyncTCP task free for the HTTP
   transfer (otherwise OTA/screenshot starve each other).
4. **Adaptive quality.** Try `JPEGE_Q_MED`; if the frame overflows 16 KB (busy/dense
   screens do), retry at `JPEGE_Q_LOW`. If it still overflows, the endpoint returns 503.
5. **Color readback mapping (panel-specific!).** On this ILI9341 the read-back 16-bit
   pixel needs a byte-swap, then maps as `hi5=Blue, mid6=Red, lo5=Green`; JPEGENC wants
   RGB888 in **B,G,R** byte order. This was found empirically by sampling known theme
   colors. **Expect to re-derive this per panel** — if your first screenshot has blue/orange
   swapped or looks like noise, this mapping is why.

`/api/screen.jpg` streams `_jpg` via `beginChunkedResponse` (chunked memcpy out of the
buffer). `scripts/shot.ps1` just `curl`s it to `.pio/shot.jpg`, which the agent then opens.

---

## 3. On-device: control + status API

Served by `src/services/WebPortal.cpp` (ESPAsyncWebServer, the ESP32Async fork). All on
the LAN, **no auth** (deliberately, so the tooling works — see §7):

| Route | Method | Purpose |
|---|---|---|
| `/api/screen.jpg` | GET | full-res JPEG of the live screen |
| `/api/tap?x=&y=`  | GET | inject a touch at screen px (x,y) |
| `/api/swipe?dir=left\|right` | GET | inject a page swipe |
| `/api/status`     | GET | JSON: fw, board, heap, heapBlk, wifi, page, mode, provider counts |
| `/api/settings`   | GET/POST | read / patch the settings JSON |
| `/remote`         | GET | a browser page: live screen img + click-to-tap + swipe buttons |
| `/update`         | GET/POST | ElegantOTA v3 upload UI (basic-auth `admin:overhead`) |

`tap`/`swipe` push synthetic events into the same input queue the touch driver feeds, so
the firmware can't tell them from a real finger.

### Navigation cheatsheet (this app's UX — yours will differ)
- `swipe?dir=right` → **next** page (page index +1, wraps); `left` → previous (−1, wraps).
- Center-third tap (x≈160 on a 320-wide screen) **cycles sub-views** within a page.
- `/api/status` reports `"page":N` — poll it to confirm where you landed.
- **Director caveat:** in `"mode":"auto"` the device auto-switches pages on a timer; it
  will drift off the page you set. Navigate, screenshot **immediately**, and don't trust the
  page to stay put across a few seconds. (A tap flips it toward manual via inactivity.)

---

## 4. On-device: OTA flash

[ElegantOTA v3] mounted at `/update`. `scripts/ota.ps1`:
1. MD5s the built `.pio/build/<env>/firmware.bin`.
2. `POST /ota/start?mode=fr&hash=<md5>` (basic-auth).
3. `POST /ota/upload` with the binary as multipart.
4. Prints `upload=200` on success.

**Flake to expect:** if a `/remote` browser tab is open (or a screenshot is mid-transfer)
the single AsyncTCP task is saturated and the upload returns `100`/`400`. **Just retry after
a short pause** — `upload=200` then. Don't "fix" it by streaming the screenshot row-by-row;
that blocks the async task and starves OTA worse (already tried).

After a successful flash the device **reboots** (uptime resets, page resets per Director).

---

## 5. Replicating on a new project — checklist

1. **Build green** with the venv `platformio.exe run -e <env>`.
2. Add **ElegantOTA** → you can flash over WiFi. (Biggest single win; do this first.)
3. Add a **JPEG screenshot endpoint**: boot-allocate one ~16 KB buffer, encode the
   framebuffer/panel-readback MCU-by-MCU with JPEGENC, serve via chunked response.
   Re-derive the panel's color readback mapping by sampling a known color.
4. Add **`/api/tap`, `/api/swipe`, `/api/status`** that inject into your input queue and
   dump key state as JSON.
5. Write two **single-line host scripts** (flash, screenshot) so they can be allow-listed
   once — no flags that change per call.
6. Loop: edit → build → `ota.ps1` → `shot.ps1` → read the jpg → `curl` tap/swipe →
   `/api/status` → commit. Verify visually every change.

---

## 6. The binding constraint: no-PSRAM RAM budget

This board has **no PSRAM**, ~320 KB heap, and it shapes everything above:

- **TLS needs a ~40 KB contiguous block.** `NetClient` *skips* an HTTPS fetch when the
  largest free block < 42 KB (avoids an OOM heap-corruption crash); providers serve stale
  and retry. So any large transient allocation can knock providers offline.
- That's why the screenshot buffer is **16 KB, boot-allocated, never freed** (§2.2). A
  38 KB BMP buffer — or a runtime malloc of the JPEG buffer — dropped the largest block
  below the TLS floor and starved the aircraft/space-weather feeds.
- Data is trimmed to fit: satellite TLEs are **watchlist-only**, aircraft capped at 24,
  METARs at 12. On the PSRAM CrowPanel these caps can be lifted (board-conditional).
- **Lesson that bit hard:** `Settings::backfillDefaults()` re-adds any missing keys on every
  load, because a stale `settings.json` + the web form's "save all" once blanked keys to
  0/false and broke alerts/backlight/timeouts. Never let a form write keys it didn't load.

---

## 7. Security note (intentional gap)

`/api/tap|swipe|screen.jpg|settings` are **open on the LAN** by design — auth on them would
break this exact tooling and the `/remote` page. Only OTA (`/update`) is basic-auth'd.
Gating the API is a tracked backlog item; do it last, and keep a local bypass for the loop.
