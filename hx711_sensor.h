// hx711_sensor.h
#pragma once
#include "HX711.h"
#include <Preferences.h>

// ===== PIN CONFIG =====
#define LOADCELL_DOUT_PIN   4
#define LOADCELL_SCK_PIN    5

// ===== CALIBRATION =====
// This will be overridden by flash if CALIBRATE command was used
#define CALIBRATION_FACTOR      100.3860f
#define CALIBRATION_VERSION     1
#define LEGACY_FACTOR_CORRECTION 1.0890f
#define AUTO_TARE_ON_BOOT       0

// ===== ORIENTATION =====
// The load cell reads negative when the bin loads the beam, so flip the sign.
// Set to 0 if the mechanics ever change and load starts reading positive.
#define LOADCELL_INVERT     1

// ===== NOISE FILTER =====
#define NOISE_THRESHOLD_G   10.0f
#define READINGS_AVERAGE    50
#define READINGS_AVERAGE_FAST 8
// Tare timing. Keep the full 20-sample average for an accurate empty baseline
// (too few samples / no settle biases the zero and makes real weights read 0).
// The settle must cover the load cell's mechanical relaxation after weight is
// lifted off — 800ms was not enough and left the baseline biased high, which
// makes every later reading negative and therefore a permanent 0 on screen.
#define TARE_SAMPLES        20
#define TARE_SETTLE_MS      2000
// Post-tare check: a good tare reads back near zero. Anything beyond this means
// the baseline was captured under load / while still drifting.
#define TARE_VERIFY_SAMPLES 10
#define TARE_VERIFY_TOL_G   25.0f
// A baseline that jumps this far usually means weight was left on the scale.
// Warn about it, but never refuse the tare — a blocked tare is worse than a
// questionable one here.
#define TARE_MAX_SHIFT_G    30.0f
#define MAX_WEIGHT_DELTA_G  2000.0f

// ===== RAW SAMPLE FILTER =====
// The HX711 read is bit-banged; anything that stretches PD_SCK mid-read (BLE
// radio activity here) corrupts individual bits. A single dropped bit 18 is
// 262144 counts ~ 2.6 kg, and read_average()'s plain mean lets one bad sample
// wreck the whole burst. Take a median instead and drop samples that sit too
// far from it. Real load changes inside one burst are far below this window.
#define MAX_MEDIAN_SAMPLES  64
#define RAW_OUTLIER_COUNTS  5000L

// ===== WEIGHT LIMITS =====
#define MAX_WEIGHT_G        20000.0f

// ===== STATE =====
extern HX711 scale;
extern bool scaleConnected;

// ===== FUNCTIONS =====
void  hx711_init();
bool  waitForHX711(int timeout = 500);
float readWeightSafe(uint8_t sampleCount = READINGS_AVERAGE);
// Always applies the new baseline; returns false only if the scale is missing.
bool  hx711_tare();
void  hx711_calibrate(float knownWeightG);
void  hx711_resetToDefault();
