#include "Config.h"
#include "Runtime.h"
#include "Hardware.h"
#include "WifiManager.h"
#include "OtaService.h"
#include "Settings.h"
#include "Display.h"
#include "MotoClock.h"
#include "Rtc8563.h"
#include "BenchSensors.h"
#include "Storage.h"
#include "LogIdentity.h"
#include "LogSleepPolicy.h"
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_attr.h>
#include <esp_system.h>
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
constexpr uint32_t BENCH_MAGIC=0x4D4D0119;
constexpr uint32_t BENCH_LOG_MAGIC=0x4D4D0131;
bool sleepLogEnabled=false,sleepLogAttempted=false,sleepLogSuccess=false;
RTC_DATA_ATTR uint32_t benchMagic=0;
RTC_DATA_ATTR uint32_t benchCycle=0;
RTC_DATA_ATTR uint64_t priorAwakeUs=0;
mm::Periodic diagnosticSchedule(5000000ULL);
mm::TimeUs nowUs() { return mm::TimeUs(esp_timer_get_time()); }

// BENCH observation only. Never controls the physical radio, display or sleep.
mm::WakeLook wakeLook;
bool observeWakeLook=false;
const char* wakeActivityName(mm::WakeActivity state) {
  switch(state) {
    case mm::WakeActivity::Look: return "LOOK";
    case mm::WakeActivity::RiderReady: return "RIDER_READY";
    case mm::WakeActivity::BatteryTest: return "BATTERY_TEST";
    case mm::WakeActivity::Maintenance: return "MAINTENANCE";
    case mm::WakeActivity::Riding: return "RIDING";
    case mm::WakeActivity::SleepPending: return "SLEEP_PENDING";
  }
  return "UNKNOWN";
}
void wakeLookObserve(mm::TimeUs now) {
  const auto gesture=hardwareTakeGesture();
  if (!observeWakeLook) return;
  // Existing BENCH OTA remains awake: rearm shadow after simulated sleep
  // for a new physical gesture, NOT a new real wake or maintenance lease.
  if (wakeLook.activity()==mm::WakeActivity::SleepPending && gesture!=mm::Gesture::None)
    wakeLook.begin(now,mm::WakeCause::Button);
  const auto previous=wakeLook.activity();
  wakeLook.step(now,gesture,false,false,hardwareGesturePending());
  if (gesture!=mm::Gesture::None || wakeLook.activity()!=previous)
    Serial.printf("[WAKE LOOK] SHADOW gesture=%u state=%s deadline_us=%llu at_us=%llu; no actuation\\n",
        unsigned(gesture),wakeActivityName(wakeLook.activity()),
        (unsigned long long)wakeLook.deadline(),(unsigned long long)now);
}

bool radiosOff() {
  wifi_mode_t mode=WIFI_MODE_NULL;
  const esp_err_t w=esp_wifi_get_mode(&mode);
  return (w==ESP_ERR_WIFI_NOT_INIT || (w==ESP_OK && mode==WIFI_MODE_NULL))
    && esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE
    && esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED;
}
void abortCountdown(const char* why) {
  benchMagic=0;
  benchCycle=0;
  priorAwakeUs=0;
  storageSleepForget();
  Serial.printf("[SLEEP] CANCEL %s; maintenance recovery\n",why);
  hardwareCancelBenchSleep();
  sensorsResume();
  storageBeginMaintenance();

  activity=Activity::Connecting;
  wifiBegin();
}
void countdownPoll() {
  const auto now=nowUs();
  if (digitalRead(WAKE_BTN_PIN)==LOW) { abortCountdown("button held"); return; }
  if (mm::log::sleepLogDue(sleepLogEnabled,sleepLogAttempted,benchCycle,now)) {
    sleepLogAttempted=true;
    // No retries, no radio, no sleep cancellation on log failure.
    sleepLogSuccess=storageSleepSample(otaBootId(),benchCycle);
  }
  if (now>=countdownEnd) {
    if (!sensorsQuiesce()) { abortCountdown("MPU sleep unverified"); return; }
    if (!displaySleepCountdown(0)) { abortCountdown("display unavailable"); return; }
    if (!radiosOff() || !hardwareArmBenchSleep()) { abortCountdown("arm failed"); return; }
    if (!hardwareButtonReleased(nowUs()) || nowUs()>mm::BENCH_AWAKE_US+1000000ULL) {
      abortCountdown("button or awake deadline"); return;
    }
    benchMagic=mm::log::sleepLogContinue(sleepLogEnabled,sleepLogSuccess,benchCycle)?BENCH_LOG_MAGIC:BENCH_MAGIC;
    priorAwakeUs=nowUs();
    Serial.printf("[SLEEP] cycle=%lu timer=30s ext0=GPIO13 LOW pre_sleep_us=%llu; TFT retained\n",
        (unsigned long)benchCycle,(unsigned long long)priorAwakeUs);
    esp_deep_sleep_start();
    return;
  }
  // Normal splash until the final five seconds of the existing awake budget.
  if (countdownEnd-now>5000000ULL) {
    displayPoll(SplashMode::BenchAwake);
    return;
  }
  if (!sensorsQuiesce()) { abortCountdown("MPU sleep unverified"); return; }
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
  mm::log::identityBoot(esp_reset_reason()==ESP_RST_DEEPSLEEP &&
      (cause==ESP_SLEEP_WAKEUP_TIMER || cause==ESP_SLEEP_WAKEUP_EXT0 || cause==ESP_SLEEP_WAKEUP_EXT1));
  hardwareBegin(nowUs());
  Serial.begin(115200);
  Serial.printf("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " wake=%d\n",int(cause));
  hardwareReportBoot();
  settingsBegin();
  clockBegin(cause==ESP_SLEEP_WAKEUP_TIMER || cause==ESP_SLEEP_WAKEUP_EXT0 || cause==ESP_SLEEP_WAKEUP_EXT1);
  if (settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench) {
    rtc8563Begin();
    sensorsBegin(cause==ESP_SLEEP_WAKEUP_TIMER || cause==ESP_SLEEP_WAKEUP_EXT0 || cause==ESP_SLEEP_WAKEUP_EXT1);
  }
  diagnosticSchedule.start(nowUs());
  otaInit();
  const bool bench=settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench;
  observeWakeLook=bench;
  const auto wakeCause=cause==ESP_SLEEP_WAKEUP_TIMER?mm::WakeCause::Timer:
      cause==ESP_SLEEP_WAKEUP_EXT0?mm::WakeCause::Button:
      cause==ESP_SLEEP_WAKEUP_UNDEFINED?mm::WakeCause::Cold:mm::WakeCause::Other;
  wakeLook.begin(0,wakeCause); // deadline includes setup, not only loop time
  if (observeWakeLook) Serial.printf("[WAKE LOOK] SHADOW boot=%u deadline_us=%llu; no actuation\\n",
      unsigned(wakeCause),(unsigned long long)wakeLook.deadline());
  const bool qualifiedTimer=esp_reset_reason()==ESP_RST_DEEPSLEEP && cause==ESP_SLEEP_WAKEUP_TIMER;
  const bool continuing=bench && qualifiedTimer &&
      (benchMagic==BENCH_MAGIC || benchMagic==BENCH_LOG_MAGIC) && benchCycle>0 && benchCycle<0xFFFFFFFFUL;
  const bool requested=bench && mm::benchSleepActive(settingsCurrent());
  sleepLogEnabled=(continuing && benchMagic==BENCH_LOG_MAGIC) ||
    (requested && settingsCurrent().benchSleepLog && cause==ESP_SLEEP_WAKEUP_UNDEFINED);
  if(!continuing || !sleepLogEnabled) storageSleepForget();
  bool consumed=!requested;
  if (requested) {
    auto next=settingsCurrent();
    next.benchSleep=false; next.benchSleepLog=false;
    consumed=settingsSave(next);
  }
  // Timer wake alone may repeat the test. Button wake, reset and power loss
  // clear the RTC test state and return to the existing OTA maintenance path.
  if (!continuing) {
    benchMagic=0;
    benchCycle=0;
    priorAwakeUs=0;
  }
  if (consumed && (continuing || (requested && cause==ESP_SLEEP_WAKEUP_UNDEFINED)) &&
      digitalRead(WAKE_BTN_PIN)==HIGH && radiosOff()) {
    ++benchCycle;
    displayBegin(false);
    if (nowUs()<mm::BENCH_AWAKE_US) {
      countdownEnd=mm::BENCH_AWAKE_US;
      activity=Activity::BenchCountdown;
      Serial.printf("[LOG] sleep commissioning=%s\n",sleepLogEnabled?"armed":"off");
      Serial.printf("[SLEEP] BENCH 30/30 cycle=%lu prior_awake_us=%llu; button cancels\n",
          (unsigned long)benchCycle,(unsigned long long)priorAwakeUs);
      return;
    }
    abortCountdown("setup exceeded budget");
    return;
  }
  benchMagic=0;
  benchCycle=0;
  priorAwakeUs=0;
  storageSleepForget();
  if (requested || continuing) Serial.println("[SLEEP] CANCEL request clear, button or radio check failed");
  if (bench) displayBegin(cause==ESP_SLEEP_WAKEUP_UNDEFINED && esp_reset_reason()==ESP_RST_POWERON);
  storageBeginMaintenance();
  wifiBegin();
}

void loop() {
  resourcesObserveLoop(nowUs());
  if (otaRebootPending()) { otaPoll(); vTaskDelay(1); return; }
  hardwarePoll(nowUs());
  wakeLookObserve(nowUs());
  if (activity!=Activity::BenchCountdown || nowUs()+5000000ULL<countdownEnd) sensorsPoll(nowUs());
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
        if (wifiConnected()) {
          if(settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench) storageRecoverMaintenance();
          otaStart(); activity=Activity::Maintenance;
        }
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
