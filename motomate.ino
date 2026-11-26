#include "Config.h"
#include "Runtime.h"
#include "Hardware.h"
#include "WifiManager.h"
#include "OtaService.h"
#include <esp_timer.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
enum class Activity { Connecting, Maintenance, Retry };
Activity activity=Activity::Connecting;
mm::TimeUs retryAt=0;
mm::TimeUs nowUs() { return mm::TimeUs(esp_timer_get_time()); }
}

void setup() {
  const auto cause=esp_sleep_get_wakeup_cause();
  hardwareBegin(nowUs());
  Serial.begin(115200);
  Serial.printf("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " BENCH wake=%d\n",int(cause));
  otaInit();
  wifiBegin();
}

void loop() {
  if (otaRebootPending()) { otaPoll(); vTaskDelay(1); return; }
  hardwarePoll(nowUs());
  switch (activity) {
    case Activity::Connecting:
      if (wifiPoll()) {
        if (wifiConnected()) { otaStart(); activity=Activity::Maintenance; }
        else { retryAt=nowUs()+30000000ULL; activity=Activity::Retry; }
      }
      break;
    case Activity::Maintenance:
      if (!wifiConnected()) { otaStop(); wifiBegin(); activity=Activity::Connecting; }
      else otaPoll();
      break;
    case Activity::Retry:
      if (nowUs()>=retryAt) { wifiBegin(); activity=Activity::Connecting; }
      break;
  }
  vTaskDelay(1);
}
