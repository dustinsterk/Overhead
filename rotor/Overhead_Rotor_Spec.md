# Overhead Companion Rotor — Implementation Spec

**For:** Claude Code
**Status:** rotor node is the deliverable. Overhead integration and the radio/Doppler node are **deferred** (see §10).

---

## 1. What this is

A small alt/az pointer that physically slews to whatever the Overhead sky dashboard is
tracking (ISS, a satellite, a planet). It is a **dumb pointer**: Overhead computes az/el
and broadcasts it; the rotor consumes the broadcast, homes itself, and tracks. No routing,
no pairing, no clock of its own.

A working single-build firmware already exists (§11). This spec is the plan to generalize it
into a configurable node that supports both the author's build (28BYJ-48 + ULN2003 + custom
gears) and other people's builds (NEMA 17 + STEP/DIR drivers + different ratios), and to
factor the wire format into a shared header so Overhead and a future radio node share one
definition.

---

## 2. Scope

**In scope (implement now):**
- Configurable stepper drivers (unipolar 4-wire *and* STEP/DIR) — §5, §6
- Self-measuring calibration for `steps_per_degree` and north offset — §7
- Shared telemetry contract in `shared/telemetry.h`, with reserved radio fields — §4
- Persistence of calibration/config to NVS — §8
- Retain existing behavior: channel hunt, homing, rate-tracking, el accel-trim, park/watchdog — §9

**Deferred (do NOT implement now — §10):**
- Overhead sender-side broadcast + wiring the rotor into the existing Overhead repo
- The separate radio/Doppler node (UV-PRO BLE control)

---

## 3. Repo layout

Monorepo, single source of truth for the wire format:

```
overhead/            # existing dashboard firmware (untouched by this task)
rotor/               # THIS task
  ├─ src/rotor.ino   # (or main.cpp under PlatformIO)
  ├─ src/config.h    # per-build tunables (§5)
  └─ platformio.ini  # or Arduino sketch layout
radio/               # deferred — placeholder only, do not implement
shared/
  └─ telemetry.h     # THE packet definition, included by every node (§4)
```

`shared/telemetry.h` is the integration seam. It is the thing that silently drifts and breaks
builds when split across repos, so it lives in exactly one place and both sides `#include` it.
**Where this tree ultimately lands relative to the current Overhead repo is deferred to
integration (§10).** For now build `rotor/` + `shared/` as a standalone, compilable unit.

---

## 4. Telemetry contract — `shared/telemetry.h`

One packet, broadcast by Overhead, consumed by all nodes. The rotor uses the pointing fields
and ignores the radio fields; the radio fields exist now so the packet shape never has to
change when the radio node arrives.

```c
#pragma once
#include <stdint.h>

#define TELEM_PROTO_VER 1

typedef struct __attribute__((packed)) {
  uint8_t  proto_ver;      // = TELEM_PROTO_VER; receiver rejects mismatch
  uint8_t  valid;          // 1 = target active/above horizon, 0 = park/idle heartbeat
  uint8_t  target_id;      // opaque id of the tracked object (0 = none)
  uint8_t  _pad;

  uint32_t seq;            // monotonic; heartbeat + staleness detection

  // --- pointing (rotor consumes) ---
  float    az;             // deg, 0..360, true-north referenced
  float    el;             // deg, horizon = 0
  float    az_rate;        // deg/s, signed
  float    el_rate;        // deg/s, signed

  // --- radio (reserved for the deferred Doppler node; rotor ignores) ---
  uint32_t base_freq_hz;   // nominal freq (Hz); uint32 covers to 4.29 GHz
  int32_t  doppler_hz;     // signed Doppler offset (Hz)
  float    range_rate_kms; // km/s; lets the radio node recompute Doppler for its own freq
} telemetry_t;
```

**Broadcast semantics (defines what the deferred sender must eventually do):**
- Sent to broadcast MAC `FF:FF:FF:FF:FF:FF` so any node hears it with no prior pairing.
- Steady rate ~2 Hz **even when idle** (`valid=0`). The heartbeat is what lets the rotor find
  and hold the channel and detect dropouts. Same packets do pointing + keepalive + beacon.
- Receiver rejects on `len != sizeof(telemetry_t)` or `proto_ver != TELEM_PROTO_VER`.

Migrate the existing `pointing_t` in the firmware to this struct.

---

## 5. Configuration surface — `rotor/src/config.h`

Keep this deliberately small. Everything downstream of the driver reduces to `steps_per_degree`
per axis, so resist a sprawling menu. Per axis (`AZ`, `EL`):

| Field | Meaning |
|---|---|
| `driver` | `DRIVER_UNIPOLAR_4WIRE` or `DRIVER_STEP_DIR` |
| pins | 4 coil pins (unipolar) **or** STEP/DIR/EN (step-dir) |
| `steps_per_deg` | default from ratio; overwritten by calibration (§7) |
| `min_deg`, `max_deg` | travel limits |
| `max_speed`, `accel` | steps/s and steps/s² |
| `invert` | flip direction |

Global: scan channel set, packet timeout, el trim gain/deadband, north offset (calibrated),
limit switch pin + active level, IMU pins/address.

Ship two ready-made config presets as comments/examples: the author's **BYJ + ULN2003** build
and a generic **NEMA 17 + A4988/TMC2209** build.

---

## 6. Driver abstraction

AccelStepper already covers both interfaces — do not hand-roll stepping:
- **Unipolar 4-wire (28BYJ-48/ULN2003):** `AccelStepper::HALF4WIRE`, pin order `(IN1,IN3,IN2,IN4)`
  (the sequencing quirk that otherwise makes the motor just buzz).
- **STEP/DIR (NEMA 17 + A4988/TMC2209):** `AccelStepper::DRIVER` with STEP/DIR pins; add an
  optional EN pin.

Select per axis from `config.h` and construct the two `AccelStepper` objects accordingly.
Note in comments that NEMA 17 needs its own motor PSU (12–24 V) and that microstepping
multiplies `steps_per_deg` — which calibration measures anyway, so the user never has to do
that math.

---

## 7. Calibration — the headline feature

Solves "I don't remember my gear ratio." The rotor measures itself and stores the result.

**Elevation `steps_per_deg` (automatic, exact):** step the el axis a known count, read the
accelerometer pitch before/after (gravity is an absolute reference), and compute
`steps_per_deg = Δsteps / Δpitch`. Repeat over a few spans and average.

**Azimuth `steps_per_deg`:**
- **If the az axis can rotate a full 360°** (limit flag hits the switch once per revolution):
  home to the switch (zero), then jog forward until the switch triggers again = exactly one
  mechanical revolution → `steps_per_deg = steps_counted / 360`. Fully automatic.
- **If cable-wrap prevents a full turn:** fall back to a guided manual cal — operator jogs to a
  physical reference mark, records, jogs 360° back to it, firmware computes the ratio.

**North offset:** operator jogs azimuth to true north once (compass/landmark) and issues
`SETNORTH`; store `NORTH_OFFSET_DEG = current mechanical az`.

**Trigger + UX:** simple serial command menu over USB — `CAL AZ`, `CAL EL`, `SETNORTH`,
`HOME`, `SHOW`, `RESET`. Optional: hold the limit switch at boot to enter calibration.
All results persist to NVS (§8) and survive reflash.

---

## 8. Persistence

Use ESP32 `Preferences` (NVS): `az_steps_per_deg`, `el_steps_per_deg`, `north_offset_deg`,
and any per-axis inversion discovered during cal. On boot, load NVS values over the `config.h`
defaults. `RESET` clears NVS back to defaults.

---

## 9. Behavior / state machine (retain + extend)

```
SCANNING ──lock──► HOMING_AZ ──► HOMING_EL ──► TRACKING ⇄ PARK
   ▲                                              │        │
   └──────────────── lost > RESCAN_MS ────────────┴────────┘
CALIBRATION: entered on demand (serial / boot hold), returns to SCANNING when done
```

- **SCANNING:** walk `SCAN_CH` (1/6/11 first), dwell ~250 ms, lock on first channel carrying a
  valid packet. Motors idle.
- **HOMING_AZ:** slew to limit switch → mechanical zero.
- **HOMING_EL:** drive to accel pitch ≈ 0 → el zero (no el limit switch needed).
- **TRACKING:** extrapolate `az/el` from last packet + rate (continuous motion, coasts through
  dropouts); el closed-loop trim against gravity to null backlash.
- **PARK:** target invalid/stale → return to home; if lost > `RESCAN_MS`, re-hunt channel.

Keep the ISR-safe copy of the packet and the `proto_ver`/`len` guards in the RX callback.

**Known limitation to leave documented in-code:** near-zenith passes drive az faster than a
BYJ can follow; the rotor lags through overhead and recovers on the far side. Cosmetic for a
pointer; do not try to "fix" it in software.

---

## 10. Deferred — do NOT implement in this task

1. **Overhead sender side.** Adding the broadcast peer (channel 0), computing `az_rate/el_rate`
   by differencing the existing ephemeris, and emitting `telemetry_t` at ~2 Hz. Also the radio
   fields (`base_freq_hz`, `doppler_hz`, `range_rate_kms`) from the same TLE math.
2. **Repo placement.** Where `rotor/` + `shared/` sit relative to the current Overhead sketch,
   and refactoring Overhead's az/el compute to feed the shared struct. Adapt during integration.
3. **Radio/Doppler node.** Separate component consuming the radio fields to steer the BTECH
   UV-PRO over BLE (likely a Pi running benlink rather than an ESP32 reimplementing the GATT
   service). `radio/` is a placeholder only.

Leave clear `// DEFERRED:` markers where the rotor's code will later meet these seams (e.g. the
shared-header include path, the reserved struct fields).

---

## 11. Reference implementation (current single-build firmware)

This compiles and runs today for the BYJ + ULN2003 build. Treat it as the baseline to
generalize: lift the packet into `shared/telemetry.h`, move tunables into `config.h`, add the
driver abstraction (§6) and calibration (§7). Behavior in §9 is already present here.

```cpp
// =============================================================================
//  Overhead Companion Rotor  —  alt/az pointer driven over ESP-NOW
//  Target: plain ESP32 (WROOM devkit). Arduino-ESP32 core 3.x.
//
//  Receives {az, el, az_rate, el_rate, valid, seq} from Overhead.
//  Azimuth : open-loop steps from limit-switch home + north offset.
//  Elevation: accel-trimmed (MPU6050 gravity vector closes the loop / homes it).
//  Motion  : rate-based — extrapolate target between packets, coast on dropout.
// =============================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <AccelStepper.h>     // Library Manager: "AccelStepper"
#include <math.h>

// ----------------------------------------------------------------------------
//  CONFIG  — fill these in for your build
// ----------------------------------------------------------------------------
// Channel hunt: rotor walks these until it hears Overhead's broadcast. 1/6/11 first
// (routers almost always use those), then the rest. No hardcoded channel needed.
static const uint8_t  SCAN_CH[]     = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
static const uint8_t  N_SCAN_CH     = sizeof(SCAN_CH) / sizeof(SCAN_CH[0]);
static const uint32_t SCAN_DWELL_MS = 250;     // listen this long per channel
static const uint32_t RESCAN_MS     = 8000;    // signal lost this long -> hunt again

// 28BYJ-48 in HALF4WIRE. NOTE the pin ORDER (IN1,IN3,IN2,IN4) — this sequencing
// quirk is why "the motor just buzzes" when wired the obvious way.
#define AZ_IN1 32
#define AZ_IN2 33
#define AZ_IN3 25
#define AZ_IN4 26
#define EL_IN1 27
#define EL_IN2 14
#define EL_IN3 12          // strapping pin — fine as output once booted, but avoid pulling high at reset
#define EL_IN4 13

#define LIMIT_PIN 34       // input-only pin is perfect for a switch (saves an output-capable GPIO)
#define LIMIT_ACTIVE LOW   // switch to GND, INPUT_PULLUP

#define I2C_SDA 21
#define I2C_SCL 22
#define MPU_ADDR 0x68

// Mechanics
static const float STEPS_PER_REV   = 4096.0f;       // 28BYJ-48 half-step, output shaft
static const float AZ_STEPS_PER_DEG = STEPS_PER_REV / 360.0f;   // ~11.378
static const float EL_STEPS_PER_DEG = STEPS_PER_REV / 360.0f;   // change if el axis has its own reduction
static const float NORTH_OFFSET_DEG = 0.0f;         // mechanical-zero (switch) -> true north; set once
static const float EL_MIN_DEG = 0.0f;
static const float EL_MAX_DEG = 90.0f;

// Speeds (steps/sec). 28BYJ stalls past ~900-1000; keep margin.
static const float MAX_SPEED   = 700.0f;
static const float ACCEL       = 1200.0f;
static const float HOME_SPEED  = 350.0f;

// Tracking / safety
static const uint32_t PACKET_TIMEOUT_MS = 2500;     // no data this long -> park
static const float    EL_TRIM_GAIN = 0.25f;         // accel closed-loop trim on el (0 = off)
static const float    EL_TRIM_DEADBAND_DEG = 0.4f;

// ----------------------------------------------------------------------------
AccelStepper azM(AccelStepper::HALF4WIRE, AZ_IN1, AZ_IN3, AZ_IN2, AZ_IN4);
AccelStepper elM(AccelStepper::HALF4WIRE, EL_IN1, EL_IN3, EL_IN2, EL_IN4);

typedef struct {
  float az, el, az_rate, el_rate;
  uint8_t valid;
  uint32_t seq;
} pointing_t;

volatile pointing_t  rxPkt   = {0};
volatile uint32_t    rxTimeMs = 0;
volatile bool        haveData = false;

enum State { SCANNING, HOMING_AZ, HOMING_EL, TRACKING, PARK };
State state = SCANNING;

uint8_t  scanIdx = 0;
uint32_t chanChangeMs = 0;
uint8_t  lockedChannel = 0;

// ----------------------------------------------------------------------------
//  MPU6050 — raw accel read, no extra libs. Returns pitch (elevation) in deg.
// ----------------------------------------------------------------------------
void mpuInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);          // PWR_MGMT_1 = 0 -> wake
  Wire.endTransmission();
}

float readPitchDeg() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                            // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  // pitch about the el axis — adjust axis mapping to how the IMU sits on the el arm
  return atan2f((float)ax, sqrtf((float)ay*ay + (float)az*az)) * 180.0f / PI;
}

// ----------------------------------------------------------------------------
//  ESP-NOW
// ----------------------------------------------------------------------------
// Core 3.x signature. (Core 2.x: void onRecv(const uint8_t* mac, const uint8_t* d, int len))
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(pointing_t)) return;
  memcpy((void*)&rxPkt, data, sizeof(pointing_t));
  rxTimeMs = millis();
  haveData = true;
}

void radioInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                                   // never associate
  esp_wifi_set_channel(SCAN_CH[0], WIFI_SECOND_CHAN_NONE);  // hunt begins here
  if (esp_now_init() != ESP_OK) { Serial.println("esp_now_init failed"); return; }
  esp_now_register_recv_cb(onRecv);
}

// ----------------------------------------------------------------------------
//  Coordinate helpers
// ----------------------------------------------------------------------------
float wrap360(float d){ while(d<0)d+=360; while(d>=360)d-=360; return d; }

long azDegToSteps(float trueAz) {
  float mech = wrap360(trueAz - NORTH_OFFSET_DEG);
  // TODO cable-wrap: if a target crosses the no-go seam, take the long way instead
  return lroundf(mech * AZ_STEPS_PER_DEG);
}
long elDegToSteps(float el) {
  el = constrain(el, EL_MIN_DEG, EL_MAX_DEG);
  return lroundf(el * EL_STEPS_PER_DEG);
}

// ----------------------------------------------------------------------------
//  Channel hunt
// ----------------------------------------------------------------------------
void setChan(uint8_t ch) {
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  chanChangeMs = millis();
}
void startScan() { scanIdx = 0; setChan(SCAN_CH[0]); state = SCANNING; }

void scan() {
  // a valid packet received since we hopped here == Overhead is on this channel
  if (haveData && rxTimeMs >= chanChangeMs) {
    lockedChannel = SCAN_CH[scanIdx];
    Serial.printf("locked on channel %u\n", lockedChannel);
    state = HOMING_AZ;
    return;
  }
  if (millis() - chanChangeMs > SCAN_DWELL_MS) {        // nothing here, hop on
    scanIdx = (scanIdx + 1) % N_SCAN_CH;
    setChan(SCAN_CH[scanIdx]);
  }
}

// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(LIMIT_PIN, INPUT_PULLUP);
  mpuInit();
  azM.setMaxSpeed(MAX_SPEED); azM.setAcceleration(ACCEL);
  elM.setMaxSpeed(MAX_SPEED); elM.setAcceleration(ACCEL);
  radioInit();
  startScan();           // find Overhead's channel before doing anything
}

// --- homing: az to the switch, el to level (accel = 0) ----------------------
void homeAz() {
  azM.setSpeed(-HOME_SPEED);                 // drive toward the switch
  if (digitalRead(LIMIT_PIN) == LIMIT_ACTIVE) {
    azM.setCurrentPosition(0);               // mechanical zero
    state = HOMING_EL;
    return;
  }
  azM.runSpeed();
}
void homeEl() {
  float pitch = readPitchDeg();
  if (fabsf(pitch) < 0.5f) {                 // level == elevation 0
    elM.setCurrentPosition(0);
    state = TRACKING;
    return;
  }
  elM.setSpeed(pitch > 0 ? -HOME_SPEED : HOME_SPEED);
  elM.runSpeed();
}

// --- tracking: extrapolate target from last packet + rate ------------------
void track() {
  noInterrupts();
  pointing_t p = (pointing_t)rxPkt;
  uint32_t t  = rxTimeMs;
  interrupts();

  if (!haveData || (millis() - t) > PACKET_TIMEOUT_MS) { state = PARK; return; }
  if (!p.valid) { state = PARK; return; }

  float dt = (millis() - t) / 1000.0f;
  float azCmd = wrap360(p.az + p.az_rate * dt);       // coast at rate between updates
  float elCmd = p.el + p.el_rate * dt;

  // elevation closed-loop trim against gravity (kills BYJ backlash on el)
  if (EL_TRIM_GAIN > 0) {
    float err = elCmd - readPitchDeg();
    if (fabsf(err) > EL_TRIM_DEADBAND_DEG) elCmd += EL_TRIM_GAIN * err;
  }

  azM.moveTo(azDegToSteps(azCmd));
  elM.moveTo(elDegToSteps(elCmd));
  azM.run(); elM.run();
}

void park() {
  if (millis() - rxTimeMs > RESCAN_MS) { startScan(); return; }  // lost -> re-hunt
  azM.moveTo(0);
  elM.moveTo(elDegToSteps(EL_MIN_DEG));
  azM.run(); elM.run();
  // when a fresh valid packet shows up, resume tracking
  if (haveData && rxPkt.valid && (millis() - rxTimeMs) < PACKET_TIMEOUT_MS) state = TRACKING;
}

void loop() {
  switch (state) {
    case SCANNING:  scan();   break;
    case HOMING_AZ: homeAz(); break;
    case HOMING_EL: homeEl(); break;
    case TRACKING:  track();  break;
    case PARK:      park();   break;
    default: break;
  }
}
```

---

## 12. Build / deps

- Arduino-ESP32 **core 3.x** (RX callback uses `esp_now_recv_info_t`; core 2.x signature noted
  in-code).
- **AccelStepper** library.
- Target: ESP32 WROOM devkit (12 GPIO needed for BYJ build; NEMA build needs fewer). ESP32-C3
  is pin-tight for the 4-wire build.
- IMU: MPU6050 (6-axis). A 9-axis part is unnecessary — the magnetometer would sit in the
  steppers' magnetic field; azimuth is handled by the limit-switch home instead.
