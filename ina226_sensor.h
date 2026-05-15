#pragma once

#include <INA226_WE.h>

// ===== I2C PINS =====
#define I2C_SDA 8
#define I2C_SCL 9

// ===== INA226 CONFIG =====
#define INA226_ADDRESS            0x40
#define SHUNT_OHM                 0.01
#define MAX_CURRENT_A             4.0    // INA226 current range calibration
#define CURRENT_DEADBAND          1.5    // Suppress noise near zero
#define CURRENT_OFFSET_MA         0      // Fixed hardware offset (adjust if needed)
#define CHARGER_VOLTAGE_THRESHOLD 4.15   // 1S pack: charger-present/near-full threshold

// ===== STATE =====
extern INA226_WE ina226;
extern bool inaConnected;

extern float busVoltage_V;
extern float loadVoltage_V;
extern float current_mA;
extern float power_mW;

// ===== FUNCTIONS =====
void ina226_init();
void ina226_read();
String ina226_getStatus();
float ina226_getDisplayCurrent();
