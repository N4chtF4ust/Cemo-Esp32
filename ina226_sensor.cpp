#include "ina226_sensor.h"
#include "max17043_sensor.h"
#include <Arduino.h>

INA226_WE ina226(INA226_ADDRESS);
bool inaConnected = false;

float busVoltage_V  = 0.0;
float loadVoltage_V = 0.0;
float current_mA    = 0.0;
float power_mW      = 0.0;

void ina226_init() {
  if (ina226.init()) {
    ina226.setResistorRange(SHUNT_OHM, MAX_CURRENT_A);
    ina226.setAverage(INA226_AVERAGE_64);
    ina226.setConversionTime(INA226_CONV_TIME_1100);
    ina226.setMeasureMode(INA226_CONTINUOUS);
    inaConnected = true;
    Serial.println("[INA226] Connected");
  } else {
    Serial.println("[INA226] NOT detected — check wiring/address");
  }
}

void ina226_read() {
  float shunt_mV = ina226.getShuntVoltage_mV();
  busVoltage_V   = ina226.getBusVoltage_V();

  // Negated so Charging = Positive, Discharging = Negative
  current_mA = -(ina226.getCurrent_mA() - CURRENT_OFFSET_MA);

  power_mW      = busVoltage_V * (current_mA / 1000.0) * 1000.0;
  loadVoltage_V = busVoltage_V + (shunt_mV / 1000.0);
}


// Returns display-ready current with deadband applied
float ina226_getDisplayCurrent() {
  return (abs(current_mA) < CURRENT_DEADBAND) ? 0.0 : current_mA;
}

// Returns "CHARGING", "DISCHARGING", "IDLE (Charger On)", or "IDLE"
String ina226_getStatus() {
  float dc = ina226_getDisplayCurrent();

  if (loadVoltage_V >= MAX17043_CHARGING_VOLTAGE_THRESHOLD_V) return "CHARGING";
  // Discharge = negative
  if (dc < 0) return "DISCHARGING";

  if (busVoltage_V > CHARGER_VOLTAGE_THRESHOLD) return "IDLE (Charger On)";
  return "IDLE";
}
