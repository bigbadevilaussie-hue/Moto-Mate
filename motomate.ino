#include "Config.h"
#include "Runtime.h"
#include "Hardware.h"
#include "WifiManager.h"
#include "OtaService.h"
#include "Settings.h"
#include "Display.h"
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
enum class Activity { Connecting, Maintenance, Retry, BenchCountdown };
Activity activity=Activity::Connecting;
mm::TimeUs retryAt=0;
mm::TimeUs countdownEnd=0;
unsigned shown=999;
constexpr mm::TimeUs COUNTDOWN_US=5000000ULL;
mm::Periodic diagnosticSchedule(5000000ULL);
mm::TimeUs nowUs() { return mm::TimeUs(esp_timer_get_time()); }

bool radiosOff() {
  wifi_mode_t mode=WIFI_MODE_NULL;
  const esp_err_t w=esp_wifi_get_mode(&mode);
  return (w==ESP_ERR_WIFI_NOT_INIT || (w==ESP_OK && mode==WIFI_MODE_NULL))
    && esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE
    && esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED;
}
void abortCountdown(const char* why, bool panelAsleep=false) {
  Serial.printf("[SLEEP] CANCEL %s; maintenance recovery\n",why);
  hardwareCancelBenchSleep();
  if (panelAsleep) displayResumePanel();
  activity=Activity::Connecting;
  wifiBegin();
}
void countdownPoll() {
  const auto now=nowUs();
  if (digitalRead(WAKE_BTN_PIN)==LOW) { abortCountdown("button held"); return; }
  if (now>=countdownEnd) {
    if (!radiosOff() || !hardwareArmBenchSleep()) { abortCountdown("arm failed"); return; }
    displaySleepPanel();
    if (!hardwareButtonReleased(nowUs()) || nowUs()>=mm::PARKED_AWAKE_LIMIT_US) {
      abortCountdown("button or awake deadline",true); return;
    }
    Serial.printf("[SLEEP] timer=300s ext0=GPIO13 LOW pre_sleep_us=%llu\n",(unsigned long long)nowUs());
    esp_deep_sleep_start();
    return;
  }
  const unsigned seconds=unsigned((countdownEnd-now+999999ULL)/1000000ULL);
  if (seconds!=shown) {
    shown=seconds;
    if (!displaySleepCountdown(seconds)) { abortCountdown("display unavailable"); return; }
    Serial.printf("[SLEEP] countdown=%u\n",seconds);
  }
}
}

void setup() {
  const auto cause=esp_sleep_get_wakeup_cause();
  hardwareBegin(nowUs());
  Serial.begin(115200);
  Serial.printf("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " wake=%d\n",int(cause));
  hardwareReportBoot();
  settingsBegin();
  diagnosticSchedule.start(nowUs());
  otaInit();
  if (settingsHealthy() && mm::benchSleepActive(settingsCurrent())) {
    auto next=settingsCurrent();
    next.benchSleep=false;
    // Commit/read back before any sleep so a reset always restores OTA access.
    if (settingsSave(next) && digitalRead(WAKE_BTN_PIN)==HIGH && radiosOff()) {
      displayBegin(false);
      if (nowUs()+COUNTDOWN_US<mm::PARKED_AWAKE_LIMIT_US) {
        countdownEnd=nowUs()+COUNTDOWN_US;
        activity=Activity::BenchCountdown;
        Serial.println("[SLEEP] one-shot BENCH countdown; request cleared");
        return;
      }
      Serial.println("[SLEEP] CANCEL setup exceeded budget");
      wifiBegin();
      return;
    }
    Serial.println("[SLEEP] CANCEL request clear, button or radio check failed");
  }
  if (settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench) displayBegin();
  wifiBegin();
}

void loop() {
  if (otaRebootPending()) { otaPoll(); vTaskDelay(1); return; }
  hardwarePoll(nowUs());
  if (settingsHealthy() && mm::verboseActive(settingsCurrent()) && diagnosticSchedule.due(nowUs())) {
    Serial.printf("[DIAG] uptime_us=%llu%s\n",(unsigned long long)nowUs(),hardwareStatus().c_str());
  }
  if (activity==Activity::BenchCountdown) {
    countdownPoll();
    vTaskDelay(1);
    return;
  }
  switch (activity) {
    case Activity::BenchCountdown: break;
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
  displayPoll(activity==Activity::Maintenance ? SplashMode::Maintenance : SplashMode::Initialising);
  vTaskDelay(1);
}
