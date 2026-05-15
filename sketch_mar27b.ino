#include <Arduino.h>
#include <Wire.h>

#include "hx711_sensor.h"
#include "dht_sensor.h"
#include "ina226_sensor.h"
#include "max17043_sensor.h"
#include "oled_display.h"
#include "command_handler.h"
#include "ble_sensor.h"       // ← NEW
#include "sleep_manager.h"    // ← NEW

// ===== I2C SCAN =====
void i2c_scan() {
  Serial.println("Scanning I2C...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(I2C_SDA, I2C_SCL);
  i2c_scan();

  ina226_init();
  delay(500);     // Give INA226 time for its first conversion (64x averaging)
  max17043_init();
  hx711_init();   // Loads saved empty baseline
  dht_init();
  oled_init();    // Must come after Wire.begin()
  ble_init();     // ← NEW: start BLE advertising

  Serial.println("\nSystem Ready");
  Serial.println("----------------------------");

  cmdHandler.begin();
  sleep_manager_init();
}

// ===== LOOP =====
void loop() {
  sleep_manager_update();
  cmdHandler.update();   // Serial commands + continuous printing
  oled_update();         // Cycles display pages every 3 s
  ble_update();          // ← NEW: notify BLE client if connected
}
