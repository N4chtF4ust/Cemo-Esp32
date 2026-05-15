#include "max17043_sensor.h"

#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

static SFE_MAX1704X fuelGauge(MAX1704X_MAX17043);

bool max17043Connected = false;
float batterySoc_percent = 0.0f;
float batteryVoltage_V = 0.0f;

void max17043_init() {
  if (fuelGauge.begin(Wire)) {
    max17043Connected = true;
    uint16_t version = fuelGauge.getVersion();
    Serial.print("[MAX17043] Connected, version: 0x");
    Serial.println(version, HEX);
    
    // 1. Detection: Is the charger active?
    bool chargingNow = false;
    float initialVoltage = fuelGauge.getVoltage();
    if (!isnan(initialVoltage) && initialVoltage >= MAX17043_CHARGING_VOLTAGE_THRESHOLD_V) {
      chargingNow = true;
    }

    // 2. ONLY quickStart if we are NOT charging.
    // quickStart is designed for a cold-boot on a stable battery.
    // If we call it while charging, the SOC will "jump" to a false high value.
    if (!chargingNow) {
      Serial.println("[MAX17043] No charger detected. Performing one-time QuickStart.");
      fuelGauge.quickStart();
      delay(500); 
    } else {
      Serial.println("[MAX17043] Charger detected. Keeping previous SOC to prevent jump.");
    }
    
    // Prime the sensor with a few reads
    for(int i=0; i<10; i++) {
      fuelGauge.getSOC();
      delay(10);
    }
    
    max17043_read();
  } else {
    max17043Connected = false;
    Serial.println("[MAX17043] NOT detected — check wiring/address");
  }
}

void max17043_read() {
  if (!max17043Connected) return;
  static bool socInitialized = false;
  static uint32_t lastReadMs = 0;
  uint32_t nowMs = millis();
  if (lastReadMs != 0 && (nowMs - lastReadMs) < MAX17043_UPDATE_INTERVAL_MS) return;
  float elapsedSec = (lastReadMs == 0) ? (MAX17043_UPDATE_INTERVAL_MS / 1000.0f)
                                       : ((nowMs - lastReadMs) / 1000.0f);
  lastReadMs = nowMs;

  float socRawPercent = fuelGauge.getSOC();
  float voltage = fuelGauge.getVoltage();

  if (isnan(socRawPercent) || isnan(voltage)) return;
  
  // Refined scaling: 0.85 is a good balance for 18650 cells.
  socRawPercent = socRawPercent / 0.85f; 
  socRawPercent = constrain(socRawPercent, 0.0f, 100.0f);
  batteryVoltage_V = voltage;

  if (batteryVoltage_V >= MAX17043_FULL_VOLTAGE_THRESHOLD_V) {
    batterySoc_percent = 100.0f;
    socInitialized = true;
    return;
  }

  if (!socInitialized) {
    batterySoc_percent = socRawPercent;
    socInitialized = true;
    return;
  }

  const float prevSoc = batterySoc_percent;
  float alpha = MAX17043_SOC_SMOOTH_ALPHA;
  
  const bool isCharging = batteryVoltage_V >= MAX17043_CHARGING_VOLTAGE_THRESHOLD_V;
  
  if (isCharging) {
    // When charging, force the SOC to move VERY slowly (0.01) so it 
    // doesn't snap to the charger voltage.
    alpha = 0.01f; 
    
    // Anti-flicker: Don't let SOC drop while charger is detected.
    if (socRawPercent < prevSoc) {
       socRawPercent = prevSoc;
    }
  }

  float smoothedSoc = prevSoc + (alpha * (socRawPercent - prevSoc));

  // Limit change per second so fast update calls don't cause rapid SOC drops.
  float delta = smoothedSoc - prevSoc;
  float maxDelta = MAX17043_MAX_DELTA_PER_SEC * max(1.0f, elapsedSec);
  if (abs(delta) > maxDelta) {
      smoothedSoc = prevSoc + (delta > 0 ? maxDelta : -maxDelta);
  }

  batterySoc_percent = constrain(smoothedSoc, 0.0f, 100.0f);
}

float max17043_getSoc() {
  return batterySoc_percent;
}
