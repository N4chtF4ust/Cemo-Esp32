#pragma once

#include "DHTesp.h"

// ===== PIN CONFIG =====
#define DHTPIN 17

// ===== READ INTERVAL =====
#define DHT_READ_INTERVAL_MS 3000

// ===== STATE =====
extern DHTesp dhtSensor;
extern TempAndHumidity sensorData;
extern bool dhtOK;

// ===== FUNCTIONS =====
void dht_init();
void dht_update();
