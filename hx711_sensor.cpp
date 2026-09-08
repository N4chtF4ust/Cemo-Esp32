// hx711_sensor.cpp
#include "hx711_sensor.h"
#include <Arduino.h>

HX711 scale;
bool scaleConnected = false;
bool calibrationDone = false;

Preferences preferences;

static float lastValidReading = -1.0f;

#define RAW_DUMP_INTERVAL_MS 3000
static unsigned long lastRawDumpMs = 0;

static float rawToGrams(long raw);

void hx711_init() {
  preferences.begin("hx711", false);
  long  savedEmptyBaseline = preferences.getLong("emptyBaseline", 0);
  bool  hasCalFactor       = preferences.isKey("calFactor");
  float savedFactor        = preferences.getFloat("calFactor", CALIBRATION_FACTOR);
  int   savedCalVersion    = preferences.getInt("calVersion", 0);

  if (!hasCalFactor) {
    preferences.putFloat("calFactor", savedFactor);
    preferences.putInt("calVersion", CALIBRATION_VERSION);
  } else if (savedCalVersion < CALIBRATION_VERSION) {
    savedFactor *= LEGACY_FACTOR_CORRECTION;
    preferences.putFloat("calFactor", savedFactor);
    preferences.putInt("calVersion", CALIBRATION_VERSION);
    Serial.print("[HX711] Calibration migrated to: ");
    Serial.println(savedFactor, 4);
  }
  preferences.end();

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_gain(128);

  if (waitForHX711(1000)) {
    scaleConnected = true;
    scale.set_scale(savedFactor);

    Serial.print("[HX711] Calibration factor: ");
    Serial.println(savedFactor, 4);

    if (!AUTO_TARE_ON_BOOT && savedEmptyBaseline != 0) {
      scale.set_offset(savedEmptyBaseline);
      Serial.println("[HX711] Connected — using saved empty baseline");
      Serial.print("[HX711] Empty baseline: ");
      Serial.println(savedEmptyBaseline);
      calibrationDone = true;
    } else {
      Serial.println("[HX711] Auto tare on startup...");
      Serial.println("[HX711] Make sure NO weight is on the scale!");
      delay(2000);
      scale.tare(20);

      long emptyBaseline = scale.get_offset();
      preferences.begin("hx711", false);
      preferences.putLong("emptyBaseline", emptyBaseline);
      preferences.end();

      Serial.println("[HX711] Empty baseline saved!");
      Serial.print("[HX711] Baseline value: ");
      Serial.println(emptyBaseline);
      calibrationDone = true;
    }

    Serial.print("[HX711] Noise threshold: ");
    Serial.print(NOISE_THRESHOLD_G);
    Serial.println(" g (readings below this show as 0)");
    Serial.print("[HX711] Current reading: ");
    Serial.print(rawToGrams(scale.read_average(READINGS_AVERAGE)), 1);
    Serial.println(" g");

  } else {
    scaleConnected = false;
    Serial.println("[HX711] NOT detected — check wiring");
  }
}

bool waitForHX711(int timeout) {
  unsigned long start = millis();
  while (!scale.is_ready()) {
    if (millis() - start > (unsigned long)timeout) return false;
    yield();
  }
  return true;
}

// Single place where raw counts become grams, so the tare check and the weight
// reads can never disagree about which direction load goes.
static float rawToGrams(long raw) {
  float grams = (raw - scale.get_offset()) / scale.get_scale();
#if LOADCELL_INVERT
  grams = -grams;
#endif
  return grams;
}

// Distribution of one raw burst. The quartiles distinguish a genuinely shifted
// signal (all four values move together) from read corruption (min/max far from
// the median) and from a channel/gain switch (bimodal: quartiles split apart).
struct RawStats {
  uint8_t discarded;
  long    median;
  long    minimum;
  long    maximum;
  long    quarter;
  long    threeQtr;
};

// Median-filtered raw read. Immune to the bit-level read corruption that a
// plain mean (scale.read_average) propagates into every sample of the burst.
static long readRawFiltered(uint8_t sampleCount, RawStats* stats) {
  if (sampleCount == 0) sampleCount = 1;
  if (sampleCount > MAX_MEDIAN_SAMPLES) sampleCount = MAX_MEDIAN_SAMPLES;

  long samples[MAX_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < sampleCount; i++) {
    samples[i] = scale.read();
    yield();
  }

  // Insertion sort — sampleCount is small (<= 64).
  for (uint8_t i = 1; i < sampleCount; i++) {
    long key = samples[i];
    int8_t j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  long median = samples[sampleCount / 2];

  // Average only the samples close to the median; a corrupted bit puts the bad
  // ones tens of thousands of counts away.
  long long sum = 0;
  uint8_t kept = 0;
  for (uint8_t i = 0; i < sampleCount; i++) {
    long deviation = samples[i] - median;
    if (deviation < 0) deviation = -deviation;
    if (deviation <= RAW_OUTLIER_COUNTS) {
      sum += samples[i];
      kept++;
    }
  }

  if (stats) {
    stats->discarded = sampleCount - kept;
    stats->median    = median;
    stats->minimum   = samples[0];                 // sorted
    stats->maximum   = samples[sampleCount - 1];
    stats->quarter   = samples[sampleCount / 4];
    stats->threeQtr  = samples[(sampleCount * 3) / 4];
  }
  if (kept == 0) return median;
  return (long)(sum / kept);
}

float readWeightSafe(uint8_t sampleCount) {
  if (!scaleConnected) return 0.0f;

  if (sampleCount == 0) sampleCount = 1;

  RawStats stats;
  long rawFiltered = readRawFiltered(sampleCount, &stats);
  float reading    = rawToGrams(rawFiltered);

  // Dump the raw distribution whenever the burst looks wrong, so the cause can
  // be read off the numbers instead of guessed at from the converted grams.
  if (stats.discarded > 0 || reading < -NOISE_THRESHOLD_G) {
    unsigned long now = millis();
    if (now - lastRawDumpMs > RAW_DUMP_INTERVAL_MS) {
      lastRawDumpMs = now;
      Serial.print("[HX711] raw n=");      Serial.print(sampleCount);
      Serial.print(" drop=");              Serial.print(stats.discarded);
      Serial.print(" min=");               Serial.print(stats.minimum);
      Serial.print(" q1=");                Serial.print(stats.quarter);
      Serial.print(" med=");               Serial.print(stats.median);
      Serial.print(" q3=");                Serial.print(stats.threeQtr);
      Serial.print(" max=");               Serial.print(stats.maximum);
      Serial.print(" offset=");            Serial.print(scale.get_offset());
      Serial.print(" -> ");                Serial.print(reading, 1);
      Serial.println(" g");
    }
  }

  // Clamp negatives
  if (reading < 0.0f) reading = 0.0f;

  // Reject absolute spikes
  if (reading > MAX_WEIGHT_G) {
    return (lastValidReading >= 0.0f) ? lastValidReading : 0.0f;
  }

  // Delta filter — only when both old and new are above noise floor
  if (lastValidReading > 50.0f && reading > 50.0f) {
    if (fabsf(reading - lastValidReading) > MAX_WEIGHT_DELTA_G) {
      return lastValidReading;
    }
  }

  // Apply noise floor
  if (reading < NOISE_THRESHOLD_G) reading = 0.0f;

  lastValidReading = reading;
  return reading;
}

bool hx711_tare() {
  if (!scaleConnected) {
    Serial.println("[HX711] Cannot tare - scale not connected");
    return false;
  }

  Serial.println("[HX711] Re-calibrating empty baseline...");

  long  previousBaseline = scale.get_offset();
  float verify           = 0.0f;
  bool  accepted         = false;

  // Two attempts: the load cell keeps relaxing for a while after weight is
  // lifted off, so a baseline captured too early sits high — and because
  // readWeightSafe() clamps negatives, that shows up as a permanent 0 rather
  // than as an obvious error. Verify the tare instead of trusting it.
  for (int attempt = 1; attempt <= 2 && !accepted; attempt++) {
    waitForHX711(1000);
    delay(TARE_SETTLE_MS);
    // Median-filtered, not scale.tare() — that uses a plain mean, so a single
    // corrupted sample would be written to flash as a permanently bad baseline.
    scale.set_offset(readRawFiltered(TARE_SAMPLES, nullptr));

    // Read back a moment later — still-drifting or under-load baselines show up
    // here as a non-zero (usually negative) value.
    verify = rawToGrams(readRawFiltered(TARE_VERIFY_SAMPLES, nullptr));
    accepted = (fabsf(verify) <= TARE_VERIFY_TOL_G);

    if (!accepted) {
      Serial.print("[HX711] Tare attempt ");
      Serial.print(attempt);
      Serial.print(" unstable (reads ");
      Serial.print(verify, 1);
      Serial.println(" g after taring) — retrying");
    }
  }

  long emptyBaseline = scale.get_offset();

  // A big jump usually means weight was left on the scale, but the tare always
  // goes through — say so and move on rather than blocking the user.
  float baselineShiftG = (emptyBaseline - previousBaseline) / scale.get_scale();
  if (previousBaseline != 0 && fabsf(baselineShiftG) > TARE_MAX_SHIFT_G) {
    Serial.print("[HX711] Note: baseline moved ");
    Serial.print(fabsf(baselineShiftG), 1);
    Serial.println(" g — tare applied anyway.");
  }

  preferences.begin("hx711", false);
  preferences.putLong("emptyBaseline", emptyBaseline);
  preferences.end();

  lastValidReading = -1.0f;

  Serial.print("[HX711] New empty baseline saved: ");
  Serial.print(emptyBaseline);
  Serial.print("  (was ");
  Serial.print(previousBaseline);
  Serial.println(")");
  Serial.print("[HX711] Reading after tare: ");
  Serial.print(verify, 1);
  Serial.println(" g");

  if (!accepted) {
    Serial.println("[HX711] Note: scale had not fully settled — tare applied anyway.");
  }
  return true;   // the baseline is always stored; warnings are advisory only
}

void hx711_calibrate(float knownWeightG) {
  if (!scaleConnected) {
    Serial.println("[HX711] Cannot calibrate - scale not connected");
    return;
  }

  Serial.println("==================================");
  Serial.println("   HX711 CALIBRATION");
  Serial.println("==================================");

  // Step 1: Tare empty scale
  Serial.println("STEP 1: Remove ALL weight from scale");
  Serial.println("Taring in 5 seconds...");
  delay(5000);
  scale.set_scale();
  scale.tare(30);
  Serial.println("Tare done!");
  Serial.println();

  // Step 2: Place known weight
  Serial.print("STEP 2: Place exactly ");
  Serial.print(knownWeightG, 0);
  Serial.println("g on the scale");
  Serial.println("Waiting 10 seconds to settle...");
  delay(10000);

  // Step 3: Read raw value
  Serial.println("Reading...");
  float rawAvg = 0;
  for (int i = 0; i < 50; i++) {
    rawAvg += scale.get_units(1);
    delay(50);
  }
  rawAvg /= 50;

  // Step 4: Calculate and save factor
  float newFactor = rawAvg / knownWeightG;

  preferences.begin("hx711", false);
  preferences.putFloat("calFactor", newFactor);
  preferences.putInt("calVersion", CALIBRATION_VERSION);
  preferences.end();

  scale.set_scale(newFactor);

  // Step 5: Re-tare with new factor
  Serial.println();
  Serial.println("STEP 3: Remove the weight now!");
  Serial.println("Re-taring in 5 seconds...");
  delay(5000);
  scale.tare(20);

  long emptyBaseline = scale.get_offset();
  preferences.begin("hx711", false);
  preferences.putLong("emptyBaseline", emptyBaseline);
  preferences.end();

  lastValidReading = -1.0f;

  Serial.println();
  Serial.println("==================================");
  Serial.println("   CALIBRATION RESULT");
  Serial.println("==================================");
  Serial.print("Raw ADC average  : ");
  Serial.println(rawAvg, 2);
  Serial.print("Known weight     : ");
  Serial.print(knownWeightG, 0);
  Serial.println(" g");
  Serial.print(">>> New factor   : ");
  Serial.println(newFactor, 4);
  Serial.println();
  Serial.println("Factor saved to flash.");
  Serial.println("Copy this into hx711_sensor.h:");
  Serial.print("#define CALIBRATION_FACTOR  ");
  Serial.println(newFactor, 4);
  Serial.println("==================================");
}

void hx711_resetToDefault() {
  preferences.begin("hx711", false);
  preferences.putLong("emptyBaseline", 0);
  preferences.putFloat("calFactor", CALIBRATION_FACTOR);
  preferences.putInt("calVersion", CALIBRATION_VERSION);
  preferences.end();

  lastValidReading = -1.0f;

  Serial.println("[HX711] Calibration reset. Restart to re-calibrate.");
}
