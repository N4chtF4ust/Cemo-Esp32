#include "dht_sensor.h"
#include <Arduino.h>

DHTesp dhtSensor;
TempAndHumidity sensorData;
bool dhtOK = false;

static unsigned long lastDHTRead = 0;

void dht_init() {
  dhtSensor.setup(DHTPIN, DHTesp::DHT11);
  delay(2000);  // DHT11 startup time
  Serial.println("[DHT11] Initialized");
}

void dht_update() {
  unsigned long now = millis();
  if (now - lastDHTRead < DHT_READ_INTERVAL_MS) return;

  lastDHTRead = now;
  sensorData = dhtSensor.getTempAndHumidity();
  dhtOK = (dhtSensor.getStatus() == 0);

  if (!dhtOK) {
    Serial.println("[DHT11] Error: " + String(dhtSensor.getStatusString()));
  }
}
