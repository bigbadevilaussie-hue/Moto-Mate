#pragma once
#include <Arduino.h>

#define MOTO_MATE_VERSION "1.00"

// Pins (ESP32-S3 Super Mini)
#define BUZZER_PIN        2   // GPIO 2
#define WAKE_BTN_PIN      13  // GPIO 13
#define RED_LED_PIN       3   // GPIO 3
#define GREEN_LED_PIN     5   // GPIO 5
#define MOSFET_PIN        5   // GPIO 5 (Shared with Green LED)

// Buzzer type
#define BUZZER_ACTIVE     1   // 1 = active (hold high), 0 = passive (PWM)
