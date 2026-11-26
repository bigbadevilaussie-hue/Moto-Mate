#include "Config.h"
#include "Runtime.h"
#include "BenchDiagnostics.h"
#include "BenchCommission.h"
#include "BenchBle.h"
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
// V1.05 USB-only commissioning; longer test deadline for radio/clock.
constexpr mm::TimeUs BENCH_WAKE_US = 90000000;
constexpr mm::TimeUs USB_SETTLE_US = 3000000;
constexpr mm::TimeUs DISPLAY_TEST_BUDGET_US = 250000;
constexpr mm::TimeUs REPORT_BUDGET_US = 10000;
mm::Coordinator runtime(BENCH_WAKE_US);
mm::Periodic reportTimer(1000000);
mm::State reportedState = mm::State::Boot;
esp_sleep_wakeup_cause_t wakeCause;
enum class BootStage { Banner, Display, Storage, RadioStart, RadioPoll, BleStart, BlePoll, RecoveryStart, RecoveryPoll, Summary };
BootStage bootStage = BootStage::Banner;
bool displayOK = false, storageOK = false, radioOK = false;
bool bleOK = false, recoveryOK = false, bleAttempted = false, recoveryAttempted = false;

mm::TimeUs nowUs() {
  return static_cast<mm::TimeUs>(esp_timer_get_time());
}

void reportMetrics() {
  const auto& m = runtime.metrics();
  Serial.printf("[LOOP] awake_us=%llu max_gap_us=%llu max_service_us=%llu overruns=%lu\n",
                static_cast<unsigned long long>(runtime.awakeUs(nowUs())),
                static_cast<unsigned long long>(m.maxGapUs),
                static_cast<unsigned long long>(m.maxServiceUs),
                static_cast<unsigned long>(m.overruns));
}

void serviceBoot(mm::TimeUs now) {
  if (runtime.awakeUs(now) < USB_SETTLE_US) return;
  const auto started = nowUs();
  mm::TimeUs budget = DISPLAY_TEST_BUDGET_US;
  switch (bootStage) {
    case BootStage::Banner:
      commissionBanner(int(wakeCause));
      bootStage = BootStage::Display;
      break;
    case BootStage::Display:
      displayOK = runBenchDisplayDiagnostics();
      bootStage = BootStage::Storage;
      break;
    case BootStage::Storage:
      budget = 2000000;
      storageOK = commissionStorage();
      bootStage = storageOK && displayOK ? BootStage::RadioStart : BootStage::Summary;
      break;
    case BootStage::RadioStart:
      commissionRadioBegin();
      bootStage = BootStage::RadioPoll;
      break;
    case BootStage::RadioPoll:
      budget = 250000;
      if (commissionRadioPoll()) {
        radioOK = commissionRadioPassed();
        bootStage = radioOK ? BootStage::BleStart : BootStage::Summary;
      }
      break;
    case BootStage::BleStart:
      budget = 3000000;
      bleAttempted = true;
      commissionBleBegin();
      bootStage = BootStage::BlePoll;
      break;
    case BootStage::BlePoll:
      budget = 3000000; // BLE teardown can be synchronous; record actual time.
      if (commissionBlePoll()) {
        bleOK = commissionBlePassed();
        bootStage = bleOK ? BootStage::RecoveryStart : BootStage::Summary;
      }
      break;
    case BootStage::RecoveryStart:
      recoveryAttempted = true;
      Serial.println("\n--- 07 / WIFI RECOVERY AFTER BLE OFF ------------------");
      commissionRadioBegin(); // existing 800 ms guard and proven Wi-Fi path
      bootStage = BootStage::RecoveryPoll;
      break;
    case BootStage::RecoveryPoll:
      budget = 250000;
      if (commissionRadioPoll()) {
        recoveryOK = commissionRadioPassed();
        bootStage = BootStage::Summary;
      }
      break;
    case BootStage::Summary: {
      const bool passed = displayOK && storageOK && radioOK && bleOK && recoveryOK;
      Serial.println("\n+================ COMMISSIONING REPORT ================+");
      Serial.printf("[RESULT] sprite=%s storage=%s wifi/clock/off=%s\n",
          displayOK?"PASS":"FAIL", storageOK?"PASS":"FAIL",
          (displayOK && storageOK) ? (radioOK?"PASS":"FAIL") : "SKIPPED");
      Serial.printf("[RESULT] BLE local=%s Wi-Fi recovery=%s; OSA host report separate\n",
          bleAttempted ? (bleOK?"PASS":"FAIL") : "SKIPPED",
          recoveryAttempted ? (recoveryOK?"PASS":"FAIL") : "SKIPPED");
      Serial.println("[RESULT] physical TFT/sensors/sleep=NOT TESTED");
      Serial.println(passed ? "[RESULT] IMPLEMENTED TESTS PASS; COMMISSIONING INCOMPLETE"
                            : "[RESULT] FAIL; inspect diagnostics above");
      Serial.println("[MODE] BENCH / deep sleep inhibited");
      Serial.println("+======================================================+");
      const auto ended = nowUs();
      runtime.recordService(ended - started, budget);
      runtime.finishBoot(ended, passed);
      reportTimer.start(ended);
      return;
    }
  }
  runtime.recordService(nowUs() - started, budget);
}

void serviceMonitoring(mm::TimeUs now) {
  // First real scheduled service: diagnostic reporting. Sensor, logging
  // and transport services will be added only with their implementations.
  if (!reportTimer.due(now)) return;
  const auto started = nowUs();
  reportMetrics();
  runtime.recordService(nowUs() - started, REPORT_BUDGET_US);
}

void reportTransition() {
  const auto state = runtime.state();
  if (state == reportedState) return;
  reportedState = state;
  switch (state) {
    case mm::State::Boot: break;
    case mm::State::Monitoring: Serial.println("[STATE] MONITORING (BENCH)"); break;
    case mm::State::SleepPending: Serial.println("[STATE] SLEEP_PENDING"); break;
    case mm::State::BenchIdle: Serial.println("[STATE] BENCH_IDLE; deep sleep inhibited"); break;
    case mm::State::Fault: Serial.println("[STATE] FAULT; bench diagnostics or wake budget failed"); break;
  }
}
} // namespace

void setup() {
  // Capture wake cause before peripheral or serial initialisation.
  wakeCause = esp_sleep_get_wakeup_cause();
  runtime.begin(nowUs());
  Serial.begin(115200);
}

void loop() {
  const auto now = nowUs();
  runtime.tick(now);
  reportTransition();
  switch (runtime.state()) {
    case mm::State::Boot:
      serviceBoot(now);
      break;
    case mm::State::Monitoring:
      serviceMonitoring(now);
      break;
    case mm::State::SleepPending:
      // Real power shutdown/holds/sleep will replace this bench-only ack.
      reportMetrics();
      runtime.acknowledgeBenchSleep();
      break;
    case mm::State::BenchIdle:
    case mm::State::Fault:
      commissionBleCancel();
      commissionRadioCancel();
      // Terminal bench states; no fake successful service or reset loop.
      break;
  }
  reportTransition();
  // Yield one RTOS tick; no fixed 50ms/1s application-loop delays.
  vTaskDelay(1);
}
