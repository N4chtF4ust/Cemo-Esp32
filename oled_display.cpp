#include "oled_display.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "hx711_sensor.h"
#include "dht_sensor.h"
#include "ble_sensor.h"   // ✅ ADD THIS

// ===== INTERNAL STATE =====
static Adafruit_SH1106G display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
static bool oled_ok = false;

static unsigned long last_update = 0;
static const unsigned long UPDATE_INTERVAL_MS = 250;

// ===== INIT =====
void oled_init() {
  if (!display.begin(OLED_ADDRESS, true)) {
    Serial.println("[OLED] Init failed — check wiring / address");
    oled_ok = false;
    return;
  }
  oled_ok = true;
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("  Sensor Node");
  display.println("  Starting...");
  display.display();
  delay(1500);
  Serial.println("[OLED] OK");
}

// ===== CLEAR =====
void oled_clear() {
  if (!oled_ok) return;
  display.clearDisplay();
  display.display(); // Push the clear buffer to screen
}

// ===== POWER OFF =====
void oled_power_off() {
  if (!oled_ok) return;
  display.clearDisplay();
  display.display();
  // Adafruit_SH1106G doesn't have a direct "powerOff" but we can send the command
  // or just clearing it is usually enough for the user experience.
  // Most OLEDs support 0xAE (Display OFF)
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00); // Command stream
  Wire.write(0xAE); // Display OFF
  Wire.endTransmission();
}

// ===== STATUS =====
void oled_show_status(const char* line1, const char* line2) {
  if (!oled_ok) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) display.println(line2);
  display.display();
}

// ===== DASHBOARD =====
static void draw_dashboard() {
  display.clearDisplay();

  // --- Title + BLE status ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WEIGHT");

  // 🔵 BLE indicator (top-right)
  if (ble_isConnected()) {
    display.fillCircle(122, 4, 3, SH110X_WHITE); // connected
  } else {
    display.drawCircle(122, 4, 3, SH110X_WHITE); // not connected
  }

  // --- Weight ---
  display.setTextSize(2);
  display.setCursor(0, 9);
  if (!scaleConnected) {
    display.print("  N/A");
  } else {
    float kg = readWeightSafe(READINGS_AVERAGE_FAST) / 1000.0f;
    display.print(kg, 3);
    display.setTextSize(1);
    display.print(" kg");
  }

  // Divider
  display.drawFastHLine(0, 28, OLED_WIDTH, SH110X_WHITE);

  // --- Temp + Humidity ---
  display.setTextSize(1);
  display.setCursor(0, 31);
  display.print("TEMP");

  display.setCursor(70, 31);
  display.print("HUMID");

  display.setCursor(0, 41);
  if (!dhtOK) {
    display.print("N/A");
  } else {
    display.print(sensorData.temperature, 1);
    display.print("C");
  }

  display.setCursor(70, 41);
  if (!dhtOK) {
    display.print("N/A");
  } else {
    display.print(sensorData.humidity, 1);
    display.print("%");
  }

  display.display();
}

// ===== UPDATE =====
void oled_update() {
  if (!oled_ok) return;

  unsigned long now = millis();
  if (now - last_update < UPDATE_INTERVAL_MS) return;
  last_update = now;

  draw_dashboard();
}
