#pragma once
#include <Arduino.h>

#define MOTO_MATE_VERSION "1.00"

// Pins (ESP32-S3 Super Mini)
#define VOLTAGE_PIN       1   // GPIO 1
#define BUZZER_PIN        2   // GPIO 2
#define RED_LED_PIN       3   // GPIO 3
#define TFT_BLK_PIN       4   // GPIO 4
#define MOSFET_PIN        5   // GPIO 5
#define I2C_SDA_PIN       6   // GPIO 6
#define I2C_SCL_PIN       7   // GPIO 7
#define TFT_RST_PIN       8   // GPIO 8
#define TFT_DC_PIN        9   // GPIO 9
#define TFT_CS_PIN       10   // GPIO 10
#define TFT_MOSI_PIN     11   // GPIO 11
#define TFT_SCLK_PIN     12   // GPIO 12
#define WAKE_BTN_PIN     13   // GPIO 13
#define GPS_RX_PIN       44   // GPIO 44 (ESP RX)
#define GPS_TX_PIN       43   // GPIO 43 (ESP TX, optional)

// Buzzer type
#define BUZZER_ACTIVE     1   // 1 = active (hold high), 0 = passive (PWM)
