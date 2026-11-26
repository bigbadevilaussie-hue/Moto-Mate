#include "BenchSleep.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Dedicated USB sleep commissioning. V1.07 diagnostics remain in their
// original modules and git history, but are not run on each B2 wake.
void setup() { sleepBenchBegin(); }
void loop() {
  sleepBenchPoll();
  vTaskDelay(1);
}
