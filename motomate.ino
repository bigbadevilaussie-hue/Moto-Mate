#include "Config.h"
#include "Runtime.h"
#include "Hardware.h"
#include "WifiManager.h"
#include "OtaService.h"
#include "OtaLease.h"
#include "Settings.h"
#include "Display.h"
#include "MotoClock.h"
#include "Rtc8563.h"
#include "BenchSensors.h"
#include "BenchBleTelemetry.h"
#include "Storage.h"
#include "LogIdentity.h"
#include "LogSleepPolicy.h"
#include <string.h>
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
enum class Activity { BenchAwake, Connecting, Maintenance, Retry, SleepFault };
Activity activity=Activity::BenchAwake;
mm::TimeUs retryAt=0, awakeEnd=0, sleepUs=mm::BENCH_SLEEP_US, maintenanceEnd=0;
unsigned shown=999;
constexpr uint32_t BENCH_MAGIC=0x4D4D0119;
constexpr uint32_t BENCH_LOG_MAGIC=0x4D4D0131;
bool sleepLogEnabled=false,sleepLogAttempted=false,sleepLogSuccess=false;
bool entryArmed=false, benchDisplay=false, intro=false;
bool bleWindow=false;
uint32_t bootCause=0;
RTC_DATA_ATTR uint32_t benchMagic=0;
RTC_DATA_ATTR uint32_t benchCycle=0;
RTC_DATA_ATTR uint64_t priorAwakeUs=0;
mm::Periodic diagnosticSchedule(5000000ULL);
mm::TimeUs nowUs() { return mm::TimeUs(esp_timer_get_time()); }

// Local USB commissioning input; no extra wakes, BLE writes or Wi-Fi entry.
void fakeAdcConsolePoll(bool writable) {
  if(!mm::ADC_SIMULATION_BUILD) return;
  static char line[16]={};
  static unsigned len=0;
  static bool overflow=false;
  for(unsigned budget=0;budget<32 && Serial.available()>0;++budget) {
    const int c=Serial.read();
    if(c<0) return;
    if(c=='\r') continue;
    if(c!='\n') {
      if(len<sizeof(line)-1 && !overflow) line[len++]=char(c);
      else overflow=true;
      continue;
    }
    if(!overflow && len) {
      line[len]=0;
      if(len==4 && !memcmp(line,"ADC?",4)) {
        uint16_t mv=0;
        if(settingsFakeAdcGet(mv)) Serial.printf("[ADC] SIM mv=%u profile=%s\n",unsigned(mv),
          settingsCurrent().profile==mm::Profile::Bench?"BENCH":"FIELD");
        else Serial.println("[ADC] unavailable");
      } else if(len>=5 && !memcmp(line,"ADC ",4)) {
        uint32_t mv=0;
        bool valid=len<=9;
        for(unsigned i=4;valid && i<len;++i) {
          if(line[i]<'0' || line[i]>'9') valid=false;
          else mv=mv*10+uint32_t(line[i]-'0');
        }
        if(!valid || mv>20000) Serial.println("[ADC] rejected: expected 0..20000 mV");
        else if(!writable) Serial.println("[ADC] rejected: sleep/OTA boundary");
        else if(!settingsFakeAdcSet(uint16_t(mv))) Serial.println("[ADC] rejected: simulator/settings unavailable");
        else Serial.printf("[ADC] ACK mv=%u\n",unsigned(mv));
      } else Serial.println("[ADC] expected ADC? or ADC <mV>");
    } else if(overflow) Serial.println("[ADC] rejected: command too long");
    len=0; overflow=false;
    return; // one command per loop
  }
}
bool radiosOff() {
  wifi_mode_t mode=WIFI_MODE_NULL;
  const esp_err_t w=esp_wifi_get_mode(&mode);
  return (w==ESP_ERR_WIFI_NOT_INIT || (w==ESP_OK && mode==WIFI_MODE_NULL))
    && esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE
    && esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED;
}
void clearCycle() {
  benchMagic=0; benchCycle=0; priorAwakeUs=0;
  storageSleepForget();
}
void beginMaintenance(mm::TimeUs now) {
  // Only a qualified physical long press can call this function.
  maintenanceEnd=now+mm::MAINTENANCE_US;
  activity=Activity::Connecting;
  clearCycle();
  sleepLogEnabled=false;
  sensorsResume();
  if (benchDisplay) displayResumePanel();
  if (!benchBleStop() || !radiosOff()) {
    Serial.println("[MAINT] refused: BLE shutdown failed; reset without lease");
    hardwareBacklight(false);
    esp_restart();
    return;
  }
  bleWindow=false;
  storageBeginMaintenance();
  Serial.printf("[MAINT] physical entry lease_us=%llu deadline_us=%llu\n",
      (unsigned long long)mm::MAINTENANCE_US,(unsigned long long)maintenanceEnd);
  wifiBegin();
}
void endMaintenance() {
  // Do not attempt an unqualified live radio handover. The software reset
  // terminates radio state and the next boot defaults to autonomous BENCH.
  if (activity==Activity::Maintenance) otaStop();
  clearCycle();
  Serial.println("[MAINT] EXPIRED; reset to autonomous cycle; no lease renewal");
  hardwareBacklight(false);
  Serial.flush();
  esp_restart();
}
void sleepFault(const char* why) {
  clearCycle();
  sleepLogEnabled=false;
  benchBleStop(); bleWindow=false;
  hardwareCancelBenchSleep();
  sensorsResume();
  activity=Activity::SleepFault;
  retryAt=nowUs()+mm::BENCH_AWAKE_US;
  shown=999;
  Serial.printf("[SLEEP] FAULT %s; radios stay off; retry in 30s\n",why);
}
void sleepPoll(mm::TimeUs now) {
  if (mm::log::sleepLogDue(sleepLogEnabled,sleepLogAttempted,benchCycle,now)) {
    sleepLogAttempted=true;
    sleepLogSuccess=storageSleepSample(otaBootId(),benchCycle);
  }
  if (now>=awakeEnd) {
    if (!sensorsQuiesce()) { sleepFault("MPU sleep unverified"); return; }
    const bool keepBacklight=benchDisplay && displaySleepCountdown(0);
    if (benchDisplay && !keepBacklight)
      Serial.println("[TFT] sleep emoji unavailable; sleep continues dark");
    if (!radiosOff()) { sleepFault("radio not off"); return; }
    // A stuck/late-held switch must never cause immediate repeated EXT0 wakes.
    bool ext0=hardwareButtonReleased(nowUs());
    if (!hardwareArmBenchSleep(sleepUs,ext0,keepBacklight)) {
      if (!ext0 || hardwareButtonReleased(nowUs())) { sleepFault("arm failed"); return; }
      hardwareCancelBenchSleep();
      ext0=false;
      if (!hardwareArmBenchSleep(sleepUs,false,keepBacklight)) { sleepFault("timer arm failed"); return; }
    }
    if (ext0 && !hardwareButtonReleased(nowUs())) {
      hardwareCancelBenchSleep();
      ext0=false;
      if (!hardwareArmBenchSleep(sleepUs,false,keepBacklight)) { sleepFault("timer rearm failed"); return; }
    }
    benchMagic=mm::log::sleepLogContinue(sleepLogEnabled,sleepLogSuccess,benchCycle)?BENCH_LOG_MAGIC:BENCH_MAGIC;
    priorAwakeUs=nowUs();
    Serial.printf("[SLEEP] cycle=%lu timer_ms=%llu ext0=%s pre_sleep_us=%llu; BENCH BL=%s\n",
        (unsigned long)benchCycle,(unsigned long long)(sleepUs/1000),
        ext0?"GPIO13 LOW":"off (button held)",(unsigned long long)priorAwakeUs,keepBacklight?"ON":"OFF");
    // USB CDC may disappear in deep sleep; avoid blocking flush.
    esp_deep_sleep_start();
    return;
  }
  const auto remain=awakeEnd-now;
  const unsigned seconds=unsigned((remain+999999ULL)/1000000ULL);
  if (remain<=5000000ULL) {
    if (!sensorsQuiesce()) { sleepFault("MPU sleep unverified"); return; }
    if (seconds!=shown) {
      shown=seconds;
      if (benchDisplay) displaySleepCountdown(seconds);
      Serial.printf("[SLEEP] countdown=%u\n",seconds);
    }
    return;
  }
  if (benchDisplay && intro && now<2000000ULL) {
    displayPoll(SplashMode::BenchAwake);
    return;
  }
  if (seconds!=shown) {
    shown=seconds;
    float temperature=NAN, humidity=NAN;
    sensorsBenchEnvironment(now,temperature,humidity);
    if (benchDisplay) displayBenchLive(seconds,hardwareBenchDisplayVoltage(now),temperature,humidity);
  }
}
}

void setup() {
  const auto resumedDeadline=otaLeaseConsume();
  const auto cause=esp_sleep_get_wakeup_cause();
  bootCause=uint32_t(cause);
  const bool deep=esp_reset_reason()==ESP_RST_DEEPSLEEP &&
      (cause==ESP_SLEEP_WAKEUP_TIMER || cause==ESP_SLEEP_WAKEUP_EXT0 || cause==ESP_SLEEP_WAKEUP_EXT1);
  mm::log::identityBoot(deep);
  hardwareBegin(nowUs());
  Serial.begin(115200);
  Serial.printf("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " wake=%d\n",int(cause));
  hardwareReportBoot();
  settingsBegin();
  const bool bench=settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench;
  const auto& cfg=settingsCurrent();
  hardwareGestureTiming(mm::TimeUs(cfg.double_click_ms)*1000ULL,mm::TimeUs(cfg.long_press_ms)*1000ULL);
  entryArmed=(deep && cause==ESP_SLEEP_WAKEUP_EXT0) || digitalRead(WAKE_BTN_PIN)==HIGH;
  clockBegin(deep);
  if (bench) {
    rtc8563Begin();
    sensorsBegin(deep);
  }
  diagnosticSchedule.start(nowUs());
  otaInit();
  const bool continuing=bench && deep && cause==ESP_SLEEP_WAKEUP_TIMER &&
      (benchMagic==BENCH_MAGIC || benchMagic==BENCH_LOG_MAGIC) && benchCycle>0 && benchCycle<0xFFFFFFFFUL;
  const bool requested=bench && mm::benchSleepActive(cfg);
  sleepLogEnabled=(continuing && benchMagic==BENCH_LOG_MAGIC) ||
      (requested && cfg.benchSleepLog && cause==ESP_SLEEP_WAKEUP_UNDEFINED);
  if (!continuing) clearCycle();
  else if (!sleepLogEnabled) storageSleepForget();
  if (requested) {
    auto next=cfg;
    next.benchSleep=false; next.benchSleepLog=false;
    if (!settingsSave(next)) { sleepLogEnabled=false; storageSleepForget(); }
  }
  benchDisplay=bench && settingsHealthy();
  storageAutoBegin(benchDisplay && !sleepLogEnabled,deep && cause==ESP_SLEEP_WAKEUP_TIMER);
  uint16_t fakeMv=0;
  if (hardwareSyntheticVoltage(fakeMv))
    Serial.printf("[ADC] SYNTHETIC voltage_mv=%u profile=%s; USB ADC <mV>; no protection authority\n",unsigned(fakeMv),bench?"BENCH":"FIELD");
  // V1.37 also sleeps safely in unqualified recovery, no screen or automatic radio.
  awakeEnd=benchDisplay?mm::TimeUs(cfg.bench_awake_ms)*1000ULL:mm::BENCH_AWAKE_US;
  sleepUs=benchDisplay?mm::TimeUs(cfg.bench_sleep_ms)*1000ULL:mm::BENCH_SLEEP_US;
  ++benchCycle;
  intro=benchDisplay && cause==ESP_SLEEP_WAKEUP_UNDEFINED && esp_reset_reason()==ESP_RST_POWERON;
  if (benchDisplay) displayBegin(intro);
  Serial.printf("[BENCH] %s cycle=%lu awake_ms=%llu sleep_ms=%llu prior_awake_us=%llu; Wi-Fi OFF at boot; bounded BLE unless OTA continuation\n",
      benchDisplay?"NORMAL":"RECOVERY", (unsigned long)benchCycle,
      (unsigned long long)(awakeEnd/1000),(unsigned long long)(sleepUs/1000),
      (unsigned long long)priorAwakeUs);
  Serial.printf("[LOG] sleep commissioning=%s autonomous=%s\n",sleepLogEnabled?"armed":"off",benchDisplay && !sleepLogEnabled?"enabled":"disabled");
  if (!radiosOff()) sleepFault("unexpected radio state at boot");
  else if (benchDisplay && resumedDeadline>nowUs()) {
    maintenanceEnd=resumedDeadline;
    activity=Activity::Connecting;
    clearCycle(); sleepLogEnabled=false;
    storageBeginMaintenance();
    Serial.printf("[MAINT] OTA continuation remaining_ms=%llu; original lease only\n",(unsigned long long)((maintenanceEnd>nowUs()?maintenanceEnd-nowUs():0)/1000));
    if (nowUs()<maintenanceEnd) wifiBegin();
    else endMaintenance();
  }
  else if (benchDisplay && !sleepLogEnabled && awakeEnd>nowUs()+7000000ULL)
    bleWindow=benchBleBegin(otaBootId());
}

void loop() {
  const auto now=nowUs();
  resourcesObserveLoop(now);
  if (otaRebootPending()) { otaPoll(); vTaskDelay(1); return; }
  hardwarePoll(now);
  const bool adcWriteWindow=!otaBusy() && ((activity==Activity::BenchAwake && now<awakeEnd && awakeEnd-now>6000000ULL) ||
      (maintenanceEnd && now<maintenanceEnd && maintenanceEnd-now>5000000ULL));
  fakeAdcConsolePoll(adcWriteWindow);
  const auto gesture=hardwareTakeGesture();
  if (!entryArmed && hardwareButtonReleased(nowUs())) entryArmed=true;
  const bool normal=activity==Activity::BenchAwake || activity==Activity::SleepFault;
  if (gesture!=mm::Gesture::None) {
    Serial.printf("[BUTTON] gesture=%u at_us=%llu\n",unsigned(gesture),(unsigned long long)now);
    if (normal && entryArmed && gesture==mm::Gesture::Long) {
      beginMaintenance(nowUs());
    }
  }
  if (maintenanceEnd && nowUs()>=maintenanceEnd) { endMaintenance(); return; }
  if (activity!=Activity::BenchAwake || (nowUs()<awakeEnd && awakeEnd-nowUs()>5000000ULL))
    sensorsPoll(nowUs());
  if (settingsHealthy() && mm::verboseActive(settingsCurrent()) && diagnosticSchedule.due(nowUs())) {
    Serial.printf("[DIAG] uptime_us=%llu%s\n",(unsigned long long)nowUs(),hardwareStatus().c_str());
  }
  switch (activity) {
    case Activity::BenchAwake:
      if (bleWindow) {
        const auto t=nowUs();
        if (t>=awakeEnd || awakeEnd-t<=5000000ULL) {
          bleWindow=false;
          if (!benchBleStop()) { sleepFault("BLE shutdown unverified"); break; }
        } else benchBlePoll(t,benchCycle,bootCause,priorAwakeUs);
      }
      sleepPoll(nowUs());
      break;
    case Activity::SleepFault:
      if (nowUs()>=retryAt) {
        ++benchCycle; awakeEnd=nowUs()+mm::BENCH_AWAKE_US;
        activity=Activity::BenchAwake;
      }
      break;
    case Activity::Connecting:
      if (wifiPoll()) {
        if (wifiConnected()) {
          if (settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench) storageRecoverMaintenance();
          if (nowUs()<maintenanceEnd) { otaStart(maintenanceEnd); activity=Activity::Maintenance; }
        } else { retryAt=nowUs()+30000000ULL; activity=Activity::Retry; }
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
  // Single cooperative writer: after synchronous HTTP, never during OTA or final sleep countdown.
  const auto logNow=nowUs();
  const bool logAwake=activity==Activity::BenchAwake && logNow<awakeEnd && awakeEnd-logNow>6000000ULL;
  const bool logMaint=maintenanceEnd && logNow<maintenanceEnd && maintenanceEnd-logNow>5000000ULL;
  if (benchDisplay && settingsHealthy() && settingsCurrent().profile==mm::Profile::Bench &&
      !sleepLogEnabled && !settingsCurrent().benchSleep && !otaBusy() && (logAwake || logMaint))
    storageAutoPoll(logNow,otaBootId(),logMaint);
  if (maintenanceEnd && benchDisplay && !otaRebootPending() && nowUs()<maintenanceEnd)
    displayMaintenance(activity==Activity::Maintenance,maintenanceEnd);
  vTaskDelay(1);
}
