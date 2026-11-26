#include "Config.h"
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_attr.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// USB-only A/B power measurement. No display, sensors, filesystem,
// Wi-Fi or BLE initialization, and no external GPIO outputs driven.
namespace {
constexpr uint64_t AWAKE_US = 30000000ULL;
constexpr uint64_t SLEEP_US = 30000000ULL;
constexpr uint32_t MAGIC = 0x4d4d0109;
RTC_DATA_ATTR uint32_t magic = 0;
RTC_DATA_ATTR uint32_t cycle = 0;
uint64_t lastReport = 0;
bool ready = false;

uint64_t nowUs() { return uint64_t(esp_timer_get_time()); }

bool radiosOff() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  const esp_err_t err = esp_wifi_get_mode(&mode);
  return (err == ESP_ERR_WIFI_NOT_INIT || (err == ESP_OK && mode == WIFI_MODE_NULL))
      && esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE;
}
}

void setup() {
  const auto cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_TIMER || magic != MAGIC) {
    magic = MAGIC;
    cycle = 0;
  }
  ++cycle;
  Serial.begin(115200);
  ready = radiosOff();
  Serial.printf("[POWER BENCH] V" MOTO_MATE_VERSION " cycle=%lu cause=%d radios_off=%s\n",
      (unsigned long)cycle, int(cause), ready?"PASS":"FAIL");
  Serial.println("[POWER BENCH] USB ONLY / 30s awake / 30s deep sleep / repeat");
  if (!ready) Serial.println("[POWER BENCH] FAIL: unexpected radio state; reset required");
}

void loop() {
  if (ready) {
    const uint64_t now = nowUs();
    if (now >= AWAKE_US) {
      if (!radiosOff() ||
          esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL) != ESP_OK ||
          esp_sleep_enable_timer_wakeup(SLEEP_US) != ESP_OK) {
        ready = false;
        Serial.println("[POWER BENCH] FAIL: radio or timer precondition; reset required");
      } else {
        Serial.printf("[POWER BENCH] SLEEP cycle=%lu awake_us=%llu timer_us=%llu\n",
            (unsigned long)cycle, (unsigned long long)nowUs(),
            (unsigned long long)SLEEP_US);
        // Do not wait for USB CDC; it may disconnect in deep sleep.
        esp_deep_sleep_start();
      }
    } else if (now-lastReport >= 5000000ULL) {
      lastReport = now;
      Serial.printf("[POWER BENCH] AWAKE cycle=%lu elapsed_us=%llu\n",
          (unsigned long)cycle, (unsigned long long)now);
    }
  }
  vTaskDelay(1);
}
