#include "sleep_manager.h"
#include "oled_display.h"
#include "ble_sensor.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include "driver/rtc_io.h"

static unsigned long buttonPressStart = 0;
static bool buttonActive = false;

void sleep_manager_init() {
    pinMode(SLEEP_BUTTON_PIN, INPUT_PULLUP);
    
    // On wakeup, disable the "hold" that kept the pull-up active during sleep
    if (rtc_gpio_is_valid_gpio((gpio_num_t)SLEEP_BUTTON_PIN)) {
        rtc_gpio_hold_dis((gpio_num_t)SLEEP_BUTTON_PIN);
    }

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED) {
        Serial.print("[SYSTEM] Woke up. Cause: ");
        Serial.println(wakeup_reason);
    }
}
void enter_deep_sleep() {
    Serial.println("[SYSTEM] Preparing for deep sleep...");
    delay(100);

    // Shutdown peripherals
    oled_power_off();
    // Stop BLE

    BLEDevice::deinit(true);
    delay(200);

    // 2. Configure wakeup: Wake on GPIO 0 being LOW (pressed)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SLEEP_BUTTON_PIN, 0); // 0 = Low

    // 3. Force RTC pull-up and LOCK it so it stays active during hibernation
    if (rtc_gpio_is_valid_gpio((gpio_num_t)SLEEP_BUTTON_PIN)) {
        rtc_gpio_init((gpio_num_t)SLEEP_BUTTON_PIN);
        rtc_gpio_set_direction((gpio_num_t)SLEEP_BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en((gpio_num_t)SLEEP_BUTTON_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)SLEEP_BUTTON_PIN);
        rtc_gpio_hold_en((gpio_num_t)SLEEP_BUTTON_PIN); // This is key for S3
    }

    // 4. IMPORTANT: Wait for a STABLE release.
    // The pin must be HIGH (released) for at least 500ms continuously.
    Serial.println("[SYSTEM] Release button and wait...");
    unsigned long releaseTimer = millis();
    while (millis() - releaseTimer < 500) {
        if (digitalRead(SLEEP_BUTTON_PIN) == LOW) {
            releaseTimer = millis(); // Reset timer if button is touched/bounces
        }
        delay(10);
    }

    Serial.println("[SYSTEM] Going to sleep now.");
    Serial.flush();
    delay(100);
    esp_deep_sleep_start();
}

void sleep_manager_update() {
    bool isPressed = (digitalRead(SLEEP_BUTTON_PIN) == LOW);

    if (isPressed) {
        if (!buttonActive) {
            buttonActive = true;
            buttonPressStart = millis();
        } else {
            if (millis() - buttonPressStart > SLEEP_HOLD_TIME_MS) {
                // Button held long enough
                enter_deep_sleep();
            }
        }
    } else {
        buttonActive = false;
    }
}
