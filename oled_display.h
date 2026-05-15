#pragma once

#include <Arduino.h>

// ===== OLED DISPLAY =====
// Uses Adafruit SH110X (128x64, I2C)
// Install via Arduino Library Manager:
//   Adafruit SH110X
//   Adafruit GFX Library

#define OLED_ADDRESS  0x3C
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_RESET    -1

void oled_init();
void oled_clear();
void oled_power_off(); // New function
void oled_show_status(const char* line1, const char* line2 = nullptr);

// Redraws full dashboard every 1s — call from loop()
void oled_update();