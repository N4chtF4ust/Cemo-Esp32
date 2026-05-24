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
    max17043_read();
  } else {
    max17043Connected = false;
    Serial.println("[MAX17043] NOT detected — check wiring/address");
  }
}

void max17043_read() {
  if (!max17043Connected) return;

  float socRawPercent = fuelGauge.getSOC();
  float voltage = fuelGauge.getVoltage();

  if (isnan(socRawPercent) || isnan(voltage)) return;
  batterySoc_percent = socRawPercent;
  batteryVoltage_V = voltage;
}

float max17043_getSoc() {
  return batterySoc_percent;
}
