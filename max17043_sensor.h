#pragma once

#include <Arduino.h>

#define MAX17043_SOC_SMOOTH_ALPHA          0.20f
#define MAX17043_SOC_CHARGE_DIP_ALLOWANCE   0.02f
#define MAX17043_UPDATE_INTERVAL_MS         1000UL
#define MAX17043_MAX_DELTA_PER_SEC          0.02f
#define MAX17043_CHARGING_VOLTAGE_THRESHOLD_V 4.10f
#define MAX17043_FULL_VOLTAGE_THRESHOLD_V     3.86f

extern bool max17043Connected;
extern float batterySoc_percent;
extern float batteryVoltage_V;

void max17043_init();
void max17043_read();
float max17043_getSoc();
