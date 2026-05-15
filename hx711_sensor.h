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

// ===== NOISE FILTER =====
#define NOISE_THRESHOLD_G   10.0f
#define READINGS_AVERAGE    50
#define READINGS_AVERAGE_FAST 8
#define MAX_WEIGHT_DELTA_G  2000.0f

// ===== WEIGHT LIMITS =====
#define MAX_WEIGHT_G        20000.0f

// ===== STATE =====
extern HX711 scale;
extern bool scaleConnected;

// ===== FUNCTIONS =====
void  hx711_init();
bool  waitForHX711(int timeout = 500);
float readWeightSafe(uint8_t sampleCount = READINGS_AVERAGE);
void  hx711_tare();
void  hx711_calibrate(float knownWeightG);
void  hx711_resetToDefault();
