#pragma once
// =============================================================================
//  Overhead Rotor — per-build configuration (spec §5)
//
//  Everything build-specific lives here. Keep it small: downstream, each axis
//  reduces to a steps_per_degree, and calibration (§7) MEASURES that at runtime
//  and stores it in NVS (§8) — so you never have to work out a gear ratio.
//
//  Two presets ship below: 28BYJ-48 + ULN2003 (the author's build — DEFAULT,
//  active) and NEMA 17 + A4988/TMC2209 (generic STEP/DIR — commented). To use the
//  NEMA build, comment the BYJ preset and uncomment the NEMA one.
// =============================================================================

// Driver kinds (§6). Applied to the AccelStepper construction in the driver layer.
#define DRIVER_UNIPOLAR_4WIRE 0   // 28BYJ-48 / ULN2003  -> AccelStepper::HALF4WIRE
#define DRIVER_STEP_DIR       1   // NEMA 17 + A4988/TMC2209 -> AccelStepper::DRIVER

// ═══════════════════════════════════════════════════════════════════════════
//  PRESET A — 28BYJ-48 + ULN2003   (author's build, DEFAULT)
// ═══════════════════════════════════════════════════════════════════════════
#define AZ_DRIVER    DRIVER_UNIPOLAR_4WIRE
// Unipolar coil pins. The (IN1,IN3,IN2,IN4) sequencing quirk (the reason it
// otherwise "just buzzes") is applied in the driver construction (§6), not here.
#define AZ_PIN_IN1   32
#define AZ_PIN_IN2   33
#define AZ_PIN_IN3   25
#define AZ_PIN_IN4   26
#define AZ_PIN_STEP  -1           // (STEP/DIR unused for a unipolar axis)
#define AZ_PIN_DIR   -1
#define AZ_PIN_EN    -1
#define AZ_STEPS_PER_DEG  (4096.0f / 360.0f)   // 28BYJ half-step / 360 (~11.378); calibration overwrites
#define AZ_MIN_DEG    0.0f
#define AZ_MAX_DEG    360.0f
#define AZ_MAX_SPEED  700.0f      // 28BYJ stalls past ~900-1000; keep margin
#define AZ_ACCEL      1200.0f
#define AZ_INVERT     0           // 1 = flip direction

#define EL_DRIVER    DRIVER_UNIPOLAR_4WIRE
#define EL_PIN_IN1   27
#define EL_PIN_IN2   14
#define EL_PIN_IN3   12           // strapping pin — fine as output once booted
#define EL_PIN_IN4   13
#define EL_PIN_STEP  -1
#define EL_PIN_DIR   -1
#define EL_PIN_EN    -1
#define EL_STEPS_PER_DEG  (4096.0f / 360.0f)   // change if the el axis has its own reduction; calibration overwrites
#define EL_MIN_DEG    0.0f
#define EL_MAX_DEG    90.0f
#define EL_MAX_SPEED  700.0f
#define EL_ACCEL      1200.0f
#define EL_INVERT     0

// ═══════════════════════════════════════════════════════════════════════════
//  PRESET B — NEMA 17 + A4988 / TMC2209   (generic STEP/DIR, commented example)
//
//  NEMA 17 needs its OWN 12–24 V motor PSU. Microstepping multiplies steps_per_deg
//  — but calibration (§7) measures the real value, so you never do that math; the
//  numbers below are only a sane starting default. Comment PRESET A and use:
//
//    #define AZ_DRIVER   DRIVER_STEP_DIR
//    #define AZ_PIN_STEP 26
//    #define AZ_PIN_DIR  25
//    #define AZ_PIN_EN   33            // optional enable; -1 if hard-tied
//    #define AZ_PIN_IN1  -1            // (coil pins unused for STEP/DIR)
//    #define AZ_PIN_IN2  -1
//    #define AZ_PIN_IN3  -1
//    #define AZ_PIN_IN4  -1
//    #define AZ_STEPS_PER_DEG (200.0f * 16.0f * /*gear*/ 4.0f / 360.0f)  // calibration overwrites
//    #define AZ_MIN_DEG   0.0f
//    #define AZ_MAX_DEG   360.0f
//    #define AZ_MAX_SPEED 4000.0f      // NEMA + driver takes far higher step rates
//    #define AZ_ACCEL     8000.0f
//    #define AZ_INVERT    0
//    ...and EL likewise (STEP/DIR/EN pins, its own steps_per_deg / speed / accel).
// ═══════════════════════════════════════════════════════════════════════════

// ── Globals (build-agnostic) ────────────────────────────────────────────────
// Homing / azimuth limit switch
#define LIMIT_PIN     34          // input-only pin is ideal for a switch (saves an output GPIO)
#define LIMIT_ACTIVE  LOW         // switch to GND, INPUT_PULLUP

// IMU (MPU6050) — elevation reference (gravity) + homing + el auto-cal
#define I2C_SDA  21
#define I2C_SCL  22
#define MPU_ADDR 0x68

// North offset: mechanical zero (switch) -> true north. Compile-time default;
// SETNORTH (§7) measures and stores the real value to NVS (§8).
#define NORTH_OFFSET_DEG 0.0f

// Elevation closed-loop accel trim (nulls BYJ backlash against gravity)
#define EL_TRIM_GAIN         0.25f   // 0 = off
#define EL_TRIM_DEADBAND_DEG 0.4f

// Homing feed rate (steps/s)
#define HOME_SPEED 350.0f

// ESP-NOW channel hunt + staleness (§9). 1/6/11 first, then the rest.
#define SCAN_CH_LIST      {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13}
#define SCAN_DWELL_MS     250     // listen this long per channel
#define RESCAN_MS         8000    // signal lost this long -> hunt again
#define PACKET_TIMEOUT_MS 2500    // no data this long -> park
