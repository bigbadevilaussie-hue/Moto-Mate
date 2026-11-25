# V1.04 — Fresh runtime framework

Owner approved a new setup/loop framework first, then integration of working donor code where useful and clean replacement where needed. Do not import the 1262-line donor sketch wholesale. Preserve functional/protocol contracts and protected radio implementation.

## Current code

- Runtime.h: platform-independent coordinator, periodic scheduling (no catch-up bursts), monotonic microsecond timing and loop/service metrics. One state owner. Boot -> Monitoring -> SleepPending -> BenchIdle; boot failure/timeout -> Fault. The deadline starts at begin(), including initial idle/boot work. No state is retained across deep sleep yet.
- motomate.ino: reads ESP wake cause before serial init, starts coordinator, schedules one-off display diagnostics after USB settle interval without blocking setup, schedules monitoring reports, observes sleep request without actually entering sleep, yields one FreeRTOS tick per loop. No real monitoring sensors yet; BENCH monitoring is only a scheduler exercise.
- BenchDiagnostics.cpp/h: V1.03 off-screen TFT/splash test moved intact except returning actual pass/fail to coordinator. No physical panel init. Unused library includes removed from the sketch; retain dependency versions, reintroduce includes with real modules.
- tests/runtime_test.cpp: native assertions for scheduling, no catch-up burst, transitions, boot timeout, diagnostic failure, reset of metrics, overrun tracking and rejected invalid transition.

Bench-only constants: 10 s wake budget from setup, 1.5 s USB settle, 1 s report interval, 250 ms display-test and 10 ms report measurement budgets. These are provisional test parameters, not field timing policy. An overrun counter reports elapsed duration after a call returns; it does not interrupt blocked code. Cooperative deadline cannot preempt a synchronous call. Physical GPIO output setup, real wake sources, holds, button gesture handling, real sleep, field watchdog and safe transaction exemptions belong to later stages. Fault and BenchIdle remain awake on USB by design; never deploy this build on the bike.

## Verification

Native C++11 test passed in development container (g++ -std=c++11 -Wall -Wextra -Werror -pedantic; assertions enabled). Exact tested Runtime.h and tests/runtime_test.cpp Git blob hashes verified before commit. Arduino compile, physical ESP32 runtime and TFT remain pending.

Owner compile:
```bash
cd ~/Documents/Arduino/motomate && git pull --ff-only && arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=min_spiffs,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled .
```

Native test reproduction (optional):
```bash
cd ~/Documents/Arduino/motomate && c++ -std=c++11 -Wall -Wextra -Werror -pedantic -I. tests/runtime_test.cpp -o /tmp/motomate-runtime-test && /tmp/motomate-runtime-test
```

Expected serial at 115200, no board runtime yet:
```text
[BOOT] MOTO-MATE V1.04
[WAKE] cause=<actual ESP wake cause>; BENCH framework
[TFT] PASS: 160x128 RGB565 sprite and pixel readback
[SPLASH] PASS: 7 modes and invalid-reading placeholders
[STATE] MONITORING (BENCH)
[LOOP] awake_us=... max_gap_us=... max_service_us=... overruns=...
[STATE] SLEEP_PENDING
[LOOP] ...
[STATE] BENCH_IDLE; deep sleep inhibited
```

If diagnostics fail, expect FAULT instead; the state machine must not report successful monitoring. Measurements include the diagnostic service in max loop gap; not an accepted production latency figure.

## Integration contract

[operating-design.md](operating-design.md) is the latest decision record for the planned coordinator, priority work, protection, session management and host responsibilities. V1.04 remains the smaller bench-only implementation described above; do not infer these planned states are implemented.

Add real services one subsystem at a time, not placeholder functions. Each service has explicit ownership, cadence, bounded duration or safe transaction boundary, failure handling and a measured timing budget. Loop must remain responsive to button and power policy. State transitions only through coordinator; add domain states and explicit sleep inhibitors when real ride/maintenance/OTA/logging work arrives. Keep elapsed scheduling independent of wall clock. No catch-up bursts, arbitrary waits, duplicated flags or fake readings. Do not assume every protected call can become nonblocking without evidence. No ticket closed solely by native tests or compile success. Next candidate is power/wake and button, followed by clock/settings.
