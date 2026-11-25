#include "Config.h"
#include "Runtime.h"
#include "BenchDiagnostics.h"
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
// V1.04 is a USB bench framework, not field firmware. These are test
// budgets only; no hardware is powered up and no deep sleep is entered.
constexpr mm::TimeUs BENCH_WAKE_US = 10000000;
constexpr mm::TimeUs USB_SETTLE_US = 1500000;
constexpr mm::TimeUs DISPLAY_TEST_BUDGET_US = 250000;
constexpr mm::TimeUs REPORT_BUDGET_US = 10000;
mm::Coordinator runtime(BENCH_WAKE_US);
mm::Periodic reportTimer(1000000);
mm::State reportedState = mm::State::Boot;
esp_sleep_wakeup_cause_t wakeCause;

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
  // Defer one-off diagnostics for USB enumeration; never block setup.
  if (runtime.awakeUs(now) < USB_SETTLE_US) return;
  Serial.println("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION);
  Serial.printf("[WAKE] cause=%d; BENCH framework\n", static_cast<int>(wakeCause));
  const auto started = nowUs();
  const bool passed = runBenchDisplayDiagnostics();
  const auto ended = nowUs();
  runtime.recordService(ended - started, DISPLAY_TEST_BUDGET_US);
  runtime.finishBoot(ended, passed);
  reportTimer.start(ended);
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
      // Terminal bench states; no fake successful service or reset loop.
      break;
  }
  reportTransition();
  // Yield one RTOS tick; no fixed 50ms/1s application-loop delays.
  vTaskDelay(1);
}
