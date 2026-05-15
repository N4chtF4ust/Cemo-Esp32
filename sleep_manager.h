#pragma once

#include <Arduino.h>

#define SLEEP_BUTTON_PIN    0   // GPIO 0 (often the BOOT button)
#define SLEEP_HOLD_TIME_MS  2000 // Hold for 2s to sleep

void sleep_manager_init();
void sleep_manager_update();
void enter_deep_sleep();
