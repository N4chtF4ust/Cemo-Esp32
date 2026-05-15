// hx711_sensor.cpp
#include "hx711_sensor.h"
#include <Arduino.h>

HX711 scale;
bool scaleConnected = false;
bool calibrationDone = false;

Preferences preferences;

static float lastValidReading = -1.0f;

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
    Serial.print(scale.get_units(READINGS_AVERAGE), 1);
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

float readWeightSafe(uint8_t sampleCount) {
  if (!scaleConnected) return 0.0f;

  if (sampleCount == 0) sampleCount = 1;
  float reading = scale.get_units(sampleCount);

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

void hx711_tare() {
  if (!scaleConnected) {
    Serial.println("[HX711] Cannot tare - scale not connected");
    return;
  }

  Serial.println("[HX711] Re-calibrating empty baseline...");
  Serial.println("[HX711] Make sure NO weight is on the scale!");
  delay(2000);
  scale.tare(20);

  long emptyBaseline = scale.get_offset();
  preferences.begin("hx711", false);
  preferences.putLong("emptyBaseline", emptyBaseline);
  preferences.end();

  lastValidReading = -1.0f;

  Serial.println("[HX711] New empty baseline saved!");
  Serial.print("[HX711] Baseline value: ");
  Serial.println(emptyBaseline);

  float testReading = scale.get_units(READINGS_AVERAGE);
  Serial.print("[HX711] Current reading after tare: ");
  Serial.print(testReading, 2);
  Serial.println(" g");
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
