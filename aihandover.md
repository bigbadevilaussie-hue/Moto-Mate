# Moto-Mate — AI Handover

**V1.07 B1 BLE commissioning owner bench PASS; field commissioning pending.** See [ble-commissioning.md](ble-commissioning.md). V1.06 is the pre-BLE verified baseline. OSA V1.40 matching bench contract required.

## Owner rule: Moto-Mate governs OSA

**OWNER RULE — 2026-11-26: Moto-Mate dictates how OSA works from now on.** The owner explicitly authorises the technical lead to fix and rework OSA to meet Moto-Mate's requirements, rather than constrain the new firmware to OSA's historical architecture. This supersedes the earlier requirement for separate scope approval for OSA engineering changes. It does not authorise arbitrary changes to Bike-Mate firmware, protected ESP32 radio sequences, or other device firmware. Preserve the battery-first prime directive, the ESP32's autonomy, and OSA's role as sole BLE broker. Preserve working services and user-facing behaviour for Bike-Mate, Fan-Mate and Audio-Mate, especially Alexa telling the owner to check the bike, existing alerts, storage and OTA workflows. Compatibility is a behavioural requirement, not a mandate to retain flawed internals. Do not silently remove features or change alert routing. No automatic deployment: use small testable changes, regression tests and an owner-controlled cutover with rollback.

Verified owner-reported baselines (2026-11-26): Moto-Mate firmware V1.06, source 6dfade2b63bbfbc112ffc07cb5ddb1f4b6854f60, compiled 981341 B flash / 46828 B static RAM, uploaded with esptool 5.6.0; repeated USB commissioning passed sprite/splash, dual OTA/LittleFS, Wi-Fi, Opal clock and Wi-Fi OFF, zero service overruns, reached BENCH_IDLE at 90 seconds. BLE/OSA handshake, physical peripherals, deep sleep and current remain untested. OSA V1.39, ca8f91eb215a1bdd2d947c181306ab7bf5f39980, deployed on iMac: /moto/status, /moto/time and /bike/status all responded successfully. Moto BLE explicitly not implemented. Existing Bike-Mate clock diagnostics preserved. Full other-device regressions not yet demonstrated.


Last updated: 2026-11-26  
Status: **V1.06 — verified USB commissioning baseline (see owner rule and verified results below).** V1.05 compiled (981357 B flash, 46828 B static RAM) and uploaded with esptool 5.6.0, boot/sprite/splash passed, but repeatable LoadProhibited at EXCVADDR 0x8. Owner decoded matching ELF f9135b5542ae49f3: EspClass::getFlashChipMode() Esp.cpp:356 -> commissionStorage() BenchCommission.cpp:175 -> loop() -> loopTask. Removed mode query only; retained size and speed diagnostics, partitions, LittleFS, Wi-Fi and clock unchanged. Verify [BOOT] MOTO-MATE V1.06 then [FLASH], [OTA], [PART], [FS] progress. V1.05 never reached storage mount or radio tests.

**V1.05 — USB commissioning implementation (compile/upload PASS; runtime FAIL as above).** Serial banner, hardware/OTA/LittleFS diagnostics, erased-only filesystem provisioning, DHCP Wi-Fi bench test and Opal LAN/NTP clock test, Wi-Fi shutdown. BLE/OSA explicitly pending (protected manager dependencies); no physical TFT, GPIO outputs, field sleep or sensor tests. Wi-Fi bench requests 20 dBm ceiling; no production TX change. V1.04 owner compile/upload and full runtime PASS: 342389 B flash, 19888 B static RAM, zero overruns on complete capture (max gap 8005 us, max service 6680 us, deadline 10000137 us). Earlier partial run had one 100 ms overrun. Use incremental compile (no --clean), upload with esptool 5.6.0 override `upload.pattern`, CoolTerm 115200 USB CDC; disconnect it before upload. No credentials in Git. See [runtime-framework.md](runtime-framework.md).  
Read this file first when starting a new Moto-Mate chat, then read [operating-design.md](operating-design.md) (latest owner decisions, architecture, maintenance workflow, priorities, GUI/OSA audit and predictive diagnostics), inspect current Git HEAD and the relevant source files. This is the owner's latest direction, not a substitute for live repository contents.

## Latest owner decision: parked wake budget and transport freedom

**AGREED owner direction (2026-11-26):** routine parked operation is 300 seconds in deep sleep, wake, perform necessary work and return to deep sleep in under 10 seconds; faster is better. The 10-second target covers boot, sensor acquisition, assessment, logging, due communications and shutdown, not merely the application loop. Ride, maintenance, PANIC/FAULT and safe transaction completion require distinct budgets; do not force sleep mid-flash or unsafe storage commit. Defer unfinished noncritical work persistently at safe boundaries rather than extend ordinary parked wakes. Five minutes means sleep duration, not start-to-start period. Exact subphase allocations and exception deadlines remain design proposals.

**AGREED:** donor BLE/Wi-Fi choices and scheduled 04:00 file collection were motivated by power consumption, not immutable requirements. Transport, cadence, batching, clock sync and fallback may change based on measured total energy per successful operation, reliability and diagnostic needs. Compare BLE and Wi-Fi on the actual S3 including boot, discovery/association, transfer, ACK, retries and shutdown; no assumption that lower instantaneous current means lower total energy. No radio changes or protected donor implementation changes are made by this decision record.

**DESIGN — instrumentation:** record monotonic microseconds per boot/wake phase, total awake duration at sleep entry, wake cause, attempted/completed/deferred work, radio/transfer timings and deadline misses. Persist compact counters and last completed wake summary; report to OSA on next successful contact without extra wake/radio solely for statistics. Track minimum, median, p95, maximum and fraction under target on host from actual samples; distinguish device monotonic time from host UTC and measure full boot-to-sleep and whole-board energy independently at supply input. OSA displays last observed state and expected next wake, not fabricated live sleep state. Start with no-work baseline, then sensors, BLE/Wi-Fi, backlog and unavailable OSA cases. Bench USB settling and verbose serial diagnostics are not representative field timing.

## 0. Project Prime Directive — SAVE THE BATTERY AT ALL COSTS

The owner's overriding requirement: "Save the Battery at all costs. dont let the tool be the bigger drain. collect all data to diagnose the battery and chanrging system."

Moto-Mate is a battery and charging-system diagnostic instrument. It must not become a significant parasitic load or contribute to the problem it is diagnosing. Battery preservation takes priority over convenience features, display uptime, connectivity and unnecessary activity.

Engineering implications (implementation subject to owner direction and evidence):
- Minimise total current at the AGM battery terminals, including regulators, peripherals, leakage, LEDs, GPS, display, and radio activity; the existing parked deep-sleep target is <=1 mA, not yet verified on Moto-Mate.
- Account for wake-up energy and radio/upload retries as well as steady sleep current; measure average parked consumption over representative wake cycles. Prevent indefinite awake states or retry loops when parked.
- Capture diagnostically useful battery voltage, timestamps, temperature, rest/discharge trends, cranking voltage sag, charging voltage and ride/engine-state transitions where the hardware and sampling permit. Preserve useful raw readings and existing diagnostic history. Do not claim to measure current, capacity or state of health directly without suitable hardware and evidence.
- Preserve logs across sleeps and failed uploads; defer nonessential activity when battery preservation requires it. Retain the proven donor software wherever possible and do not modify protected BLE/Wi-Fi code without demonstrated necessity and discussion.
- Validate the effect of changes on the battery budget before accepting them. Exact low-voltage cutoffs, sampling cadence changes and retry limits require review against the donor and owner agreement; do not invent or implement them in this handover.

## 1. Major project rule: CLEAN FIRMWARE PORT

**Latest precedence — 2026-11-26.** Owner direction (2026-11-26): this is the opportunity to remove accumulated firmware technical debt. Bike-Mate is the functional reference, not a line-count or implementation constraint. The previous approximately 95% reuse target is no longer a firmware acceptance criterion. Reuse clean, proven implementations; replace unnecessarily complicated, blocking, duplicated or obsolete implementations where justified by the audit and verified against the required behaviour. Do not copy bad code merely to preserve reuse. Preserve functional and host/data compatibility, battery-first policy, core 2.0.17, and the explicitly protected BLE/Wi-Fi implementation and proven recovery sequence. Protected changes still require separate owner approval. GUI remains essentially a straight copy. This is not permission to discard live features, rewrite proven code for style, or modify the donor firmware without scope approval. OSA engineering is authorised by the owner rule above.

The owner subsequently explicitly approved fresh setup/loop first, adding working donor implementations only as needed. See operating-design.md; the earlier direct-port directions below are historical context, subordinate to this clarification.

Moto-Mate MkII is Bike-Mate running on a different ESP32 with new peripheral modules. The owner's target is to reuse approximately 95% of both existing ESP32 firmware and GUI code. This is a target, not a measured percentage.

- Bike-Mate is the software donor and working reference. The owner confirms it was physically built, installed on the bike and worked; Moto-Mate exists because the owner wanted a better design, not because Bike-Mate failed.
- Moto-Mate is the new hardware platform and destination repository.
- The GUI is essentially a straight copy, not a new application or redesign.
- Preserve proven logic, interfaces, behaviour, timings, data formats and communication protocols. Adapt only what the new hardware or agreed project requirements actually demand.
- This explicitly supersedes any older clean-slate **software** wording in Moto-Mate README.md or project.md. The new hardware design remains distinct.
- Do not start porting or coding until the owner requests a specific task. The handover commit does not authorize starting the port now.

### Cleaned and optimised direct port — owner direction 2026-11-25

The owner now explicitly requests auditing and cleaning ESP32 code as it is ported, not copying accumulated dead code, duplicate paths, useless loops or inconsistent naming. Preserve working functionality and interfaces; remove verified dead code, fix evidenced faults and optimise measurable battery/CPU/flash costs in small tested module patches. This refines the 95% reuse intent, not the protected BLE/Wi-Fi rules. No GUI audit or rewrite requested. The full static donor audit and 47 open tickets are in [TODO.md](TODO.md), donor V6.49 commit bc5766c. Read the backlog before porting any module; source findings are not runtime test results. BLE/Wi-Fi protected changes still require demonstrated need and discussion. Owner confirms Bike-Mate asleep draw was 0.001 A (1 mA) on the bench PSU; Moto-Mate must match or beat this at supply input and measure complete wake-cycle energy.

## 2. Ownership and workflow

**Read the complete owner rules in [README.md — Rules](README.md#rules) before doing any work.** All seven original rule categories are preserved verbatim there, with explicit Moto-Mate precedence for GitHub patch ownership, cd-first shell commands, versioning and the S3 toolchain. Do not rely on the abbreviated workflow bullets below instead of reading the full rules.

ChatGPT is the technical lead for Moto-Mate and is responsible for performing all requested patching and porting through GitHub, rather than routinely handing the owner scripts or manual edits. The owner directs priorities and approves scope; the owner handles physical hardware and local testing as needed. This supersedes the older README wording that the owner edits all files himself.

GitHub access to all three repositories was verified with read, write and admin permissions during this session. Recheck availability in a new chat. Access is not permission to make unrelated changes, deploy firmware or modify donor repositories without instruction.

- Read current files and Git history before editing; never guess an anchor or patch stale source.
- Diagnose with evidence, propose the cheapest confirming test when needed, and do not patch a theory.
- Keep patches small and focused, one subsystem/file when practical. Preserve existing fixes and retries.
- Follow existing firmware version bump convention for code patches and verify the boot version. A docs-only handover does not bump firmware version.
- State exactly what changed, commit reference, what was actually tested and what remains unverified. Give a specific verification step.
- Do not pretend a GitHub commit has been pulled, built, flashed or deployed on the iMac.
- If local shell work is unavoidable, follow README patch discipline: zsh multiline/history-expansion issues, Python patch scripts under /tmp, fail on missing anchors. Prefer direct GitHub edits when requested.
- Every terminal command provided to the owner must start with cd. Verified local Moto-Mate repository: /Users/nick/Documents/Arduino/motomate (origin is the Moto-Mate GitHub repository).
- Keep replies direct, practical, and concise. No unsolicited lectures or work/rest suggestions.
- Maintain this handover as project decisions and baselines change; use the latest Git state rather than treating old snapshots as current.
- Never commit or disclose Config.local.h, credentials or tokens. Verify ignore rules before any local configuration is added.

## 3. Repositories and baseline revisions

These are the reviewed revisions, not permanent pins or claims about the currently flashed board.

| Role | Repository | Reviewed baseline |
| --- | --- | --- |
| Destination | [Moto-Mate](https://github.com/bigbadevilaussie-hue/Moto-Mate) | main, 36bbb253622e65b85f1ad68cf309220756c684b9, 2026-11-26, firmware V1.04 (latest code commit; docs may be newer) |
| Software donor | [Bike-Mate](https://github.com/bigbadevilaussie-hue/Bike-Mate) | main, bc5766c319da25622db1f2c86b58339afd07d5ad, 2026-11-03, firmware V6.49 |
| Integration server | [OSA-Server](https://github.com/bigbadevilaussie-hue/OSA-Server) | main, 434b65d6bf74734f8b4b13224472c53ad78f042e, 2026-10-30, V1.33 |

Bike-Mate V6.49 includes the two-second engine-stop debounce. Recent donor history also includes RTC settimeofday, simplified sleep interval/settings, and TinyGPSPlus. Read the current donor implementation before proposing changes, rather than reconstructing behaviour from old documentation.

## 4. Non-negotiable protected code and toolchain

**BleManager.cpp / BleManager.h are protected.** The owner spent weeks getting BLE working. Do not refactor, optimise, rewrite, switch libraries, or alter timing, callbacks, UUIDs, packet framing, handshake, ACK handling, retry behaviour or file transfer semantics for cosmetic reasons. If a change is demonstrably required by the S3 or agreed functionality, identify evidence and discuss it with the owner before touching protected code. Preserve OSA and GUI compatibility end to end.

**Arduino ESP32 core 2.0.17 ONLY.** The owner explicitly locked this because of macOS toolchain and NimBLE/BLE compatibility problems. Do not upgrade to 3.x or migrate BLE stacks; the V1.03 sketch included Arduino BLEDevice/BLEServer/BLEUtils/BLE2902 headers; V1.04 removed unused includes pending real subsystem integration. The reference to NimBLE describes past compatibility trouble, not a request to switch libraries.

**Wi-Fi/BLE coexistence is protected.** Never run both at once. Stop BLE, wait 500 ms, then start Wi-Fi as documented. The Bike-Mate WifiManager.cpp IDF teardown/reinitialisation sequence is documented as proven 72/72 on Opal. Preserve it and existing retry behaviour. Keep documented 8.5 dBm TX power baseline; validate on the new board rather than guessing.

## 5. Current development state (owner-confirmed)

- V1.04 36bbb25 is the last fully runtime-verified firmware; fresh bench-only coordinator, owner compile/upload and complete serial test PASS. No physical TFT or deep sleep test. See runtime-framework.md and latest status above.
- Last owner-compiled V1.03 b2d36d8:  386841 B flash, 20528 B static RAM, 307152 B remaining. Seven-mode splash renderer, automatic firmware version and synthetic battery readings; corrected RGB565 pixel check. Final artwork, actual runtime and TFT pending. See [port-plan.md](port-plan.md) for ordered candidates and bench gates, including main-loop audit, button, clock, OTA and BLE priorities. V1.02 history below is retained.

- V1.00 historical include-only baseline passed. V1.01 added LovyanGFX; V1.02 is the temporary off-screen display smoke test at 29808f4. Owner installed LovyanGFX 1.2.36, pulled V1.02 and pasted a successful compile/link on Arduino ESP32 core 2.0.17. Adafruit display includes remain.
- Exact verified build command:

```bash
cd ~/Documents/Arduino/motomate && arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=min_spiffs,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled .
```

- Flash: 383889 / 1966080 bytes (CLI 19%). Static RAM: 20528 / 327680 bytes (CLI 6%), 307152 bytes remaining per compiler. A 40 KiB dynamic sprite plus overhead is additional. These are temporary test binary figures, not full firmware or runtime heap measurements. The chosen 4 MB/no PSRAM/min_spiffs options are provisional, with physical capacity and final OTA/filesystem budget unconfirmed. Do not upgrade the core. See README Build status for the exact install command and test scope.
- The test configures ST7735S write-only SPI2, mode 0, 16 MHz provisional, uses Config.h GPIOs and renders a 160 x 128 RGB565 sprite in RAM with synthetic horizon and text. No display/backlight init or transfer. Expected [BOOT] MOTO-MATE V1.02 and [TFT] PASS: 160x128 RGB565 sprite and pixel readback at 115200 baud; not yet observed. No board connected during compile; runtime and physical TFT untested.
- Owner wants impressive colour graphics; LovyanGFX is the preferred candidate after the successful compile. Future lean/horizon ideas require sensor fusion and validation; not approved final functionality. Preserve battery-first policy and readable instruments.
- No donor firmware or GUI port, OSA integration or physical Moto-Mate tests have been reported. Buzzer.cpp/h remain baseline. Temporary smoke-test loop is not production power management.
- Veroboard design, custom part JSONs and V4 OpenSCAD enclosure exist. Owner confirmed actual stripboard is 24 x 56 holes. Ordered hardware, case parameters and fit checks are tracked in [hardware-inventory.md](hardware-inventory.md); final hardware and BOM remain preliminary.
- Bike-Mate GUI screenshot supplied in chat shows GUI v4.52, disconnected / NO SIGNAL. It is the visual reference, not a hardware test result. No screenshot binary is stored by this handover.

## 6. New hardware: use Moto-Mate files, not donor pin definitions

**Hardware is preliminary, not locked.** The owner uses [Stripboard Editor](https://stripboard-editor.com/) to design the schematic and stripboard. The committed `moto14wires30cuts.json` is a preliminary editor project (14 wires, 30 cuts), not an approved fabrication layout. Circuit, pin assignments and BOM remain subject to owner approval. Use the latest editor JSON for design review; do not treat an earlier audit as fabrication sign-off.

Read Config.h, motomate.ino, veroboard.md, project.md, codeprt.md, README.md and motomate_case.scad. The new platform is ESP32-S3 Super Mini HW-747, replacing the C3; 1.8-inch 128x160 ST7735 colour TFT replaces OLED; SHT40 temperature/humidity and MPU-6050 accelerometer/gyro; NEO-6M GPS; 12 V AGM supply with IRF4905 high-side switched rail; active buzzer; V4 enclosure.

Current Config.h map (ESP-side signal names):

| GPIO | Function |
| --- | --- |
| 1 | Battery voltage ADC |
| 2 | Buzzer |
| 3 | Red LED |
| 4 | TFT BLK, documented direct active-high |
| 5 | Switched MOSFET control / green LED |
| 6 / 7 | I2C SDA / SCL |
| 8 / 9 / 10 / 11 / 12 | TFT RST / DC / CS / MOSI / SCLK |
| 13 | Wake button |
| 44 / 43 | GPS RX / TX (ESP perspective; UART1) |

Existing documented constraints include 1 mA deep-sleep target, GPIO holds, MPU sleep on always-on 3.3 V, TFT sleep/backlight off, five-minute parked wake interval, LittleFS, GPS switched power, wake button modes, 4 AM log routine, and OTA via Wi-Fi maintenance mode. Preserve these as requirements to reconcile with the donor; do not blindly replace donor state-machine logic from a spec.

Known documentation inconsistencies to resolve against actual wiring and owner confirmation before affected work:
- GPS direction/pin descriptions and whether ESP TX is connected; Config.h and the latest wiring table use ESP RX 44, ESP TX 43 with a 1k series resistor on TX, while older project/code requirement text conflicts.
- TFT direct-drive BLK versus outdated IRLZ44N statements in parts/sensor wiring sections.
- Rail, diode and I2C pull-up descriptions are not fully consistent. Check actual circuit before treating any text as an electrical verification.
- README claims Config.local.h is ignored but reviewed .gitignore does not list it explicitly.
- No physical current or wiring verification was performed; the V1.02 compile/link was performed by the owner, not the AI.

## 7. Software donor map

Bike-Mate's bike_mate.ino owns the state machine, RTC, wake/sleep and setup/loop. Its modular firmware includes BleManager, DisplayManager, Sensors, GpsModule, Buzzer, RideLogger, RideStorage, WakeLogger, Settings, WifiManager, WebServer, OtaManager, DriveUpload, WifiMail and SerialBuffer. The C3-to-S3, OLED-to-TFT, NTC-to-SHT40, MPU and GPIO/power adaptations are the areas to inspect, not a license for sweeping changes. Keep the original protocol and file formats unless explicitly agreed otherwise.

The GUI is a Python/Tk application launched by bikemate.py with the package bikemate/ (app.py, ble.py, config.py, drive.py, ota.py, reports.py, ride_picker.py, ride_finder.py, dynatune_window.py, serial_window.py, state.py, weather.py, widgets.py and helpers.py). Preserve the existing GUI layout and functionality: ACC/engine/warn status, connection status, large voltage reading, temperature/storage/time, voltage and temperature graphs, settings, reports, maintenance, OTA. Branding and new telemetry changes only as requested. All 14 first-party GUI package modules, launcher and packaging script have now been reviewed at GUI V4.54, with relevant OSA paths. Findings and candidate changes are in operating-design.md; no GUI/OSA code changes or runtime tests.

## 8. OSA Server is required, same pattern as Bike-Mate

OSA is the third component, not an optional add-on. It runs on the Sonoma iMac as a Python LaunchAgent, port 9999, with Bleak for BLE central.

Architecture: ESP32-S3 <-- BLE --> OSA <-- HTTP --> GUI. OSA caches telemetry, syncs the clock, reassembles file uploads, brokers commands, manages local storage/GitHub uploads and provides notifications. Wi-Fi maintenance/OTA remains separate from the normal BLE link. Preserve the working OSA/BLE protocol.

- OSA, not the GUI, owns the single BLE connection to Bike-Mate. The GUI consumes OSA's HTTP cache.
- Existing Bike-Mate endpoints include /bike/status. OSA currently scans Bike-Mate-3 and uses DATA 26a8, TIME 26a9, OTA/commands 26ae, UPLOAD 26b1 (UUID suffixes; inspect full definitions in source).
- OSA has Bike-Mate-specific device identity, callbacks, storage paths, upload repo, firmware naming and routes. Fan-Mate uses a separate HTTP pattern; do not accidentally affect it.
- Existing OSA storage includes wakes, rides, dyna, bin and heartbeat logs. OSA handles GitHub pushing for transferred device logs. Existing GUI/firmware backup and maintenance paths should be inspected before assuming every data transfer uses the same route.
- Preserve macOS GATT caching lessons: existing docs require coordinated device-name changes when GATT changes. Do not change GATT unnecessarily.
- Decision still open: whether Moto-Mate replaces Bike-Mate in OSA or coexists alongside it; Moto-Mate naming, routes, storage and upload repository have not been chosen. Do not assume or create resources yet.
- No OSA changes or deployments have been made.

## 9. New chat startup

1. Read this handover and operating-design.md from Moto-Mate main; review README rules and the port plan.
2. Confirm current Moto-Mate, Bike-Mate and OSA revisions and inspect relevant source files.
3. Follow the direct-port rule, protected BLE/Wi-Fi code and core 2.0.17 lock.
4. Take the technical lead on requested patches/porting. Ask only for missing material hardware or test facts, and do not make speculative changes.
5. Report actual results and update this document when project decisions change.

Current next action: owner compiles V1.04 on locked toolchain and checks expected bench serial states when hardware is available. Then implement safe GPIO/sleep lifecycle and early button service in separately tested patches. Fresh coordinator approved; integrate proven donor functions into it, do not transplant donor setup/loop. No fabrication, final hardware selection, protected changes or production deployment implied.


## Owner bench validation — 2026-11-26

V1.07 firmware (01010ab) / OSA V1.40 (340ee9b): owner reported Python compile and 10/10 unit tests PASS, LaunchAgent restart, Arduino ESP32 core 2.0.17 compile 1,554,949/1,966,080 bytes flash (411,131 remaining), static RAM 67,780/327,680 bytes, upload 1,555,312 bytes verified by hash. Initial Wi-Fi 1,056 ms, Opal Date clock and Wi-Fi OFF PASS. S3 BLE init 221 ms; first connection at 760 ms, session 517 ms; second at 2,087 ms, session 484 ms; two local sessions and controller/host stack shutdown PASS at 3,028 ms. Wi-Fi recovery 1,043 ms and final Opal clock/Wi-Fi OFF PASS. OSA phase passed, error null, disconnected, same boot ID MMB1/1.07/0fd96088 both sessions; discovery 29,801 ms (includes pre-advertising wait during upload/boot) and 88 ms; connect 623/547 ms; exact challenge notification 30/36 ms; clock write and readback 50/50 ms; applied and actual epoch match requested 1795652966/1795652968; host difference -1/0 s (integer-second sampling, not long-term drift); second full host cycle 793 ms. One loop service overrun (375,248 us, max gap 376,001 us), phase not yet isolated. First monitoring report awake 14,213,438 us, includes 3 s USB settle and commissioning overhead; not a field wake measurement. BENCH end-to-end BLE/OSA and Wi-Fi recovery verified on this run. Still pending repeated reliability, field 300 s deep sleep / <10 s wake, current/energy measurements, clock holdover, physical TFT/sensors, real file transfer and other-device live regression. No production changes or version bump in this validation record.


V1.08 B2 USB sleep commissioning implemented; validation pending. See sleep-commissioning.md. V1.07 is last measured baseline. OSA V1.41 required.


## Owner B2 validation — 2026-11-26

Moto-Mate V1.08 (52b2cec), OSA V1.41 (e509b36): Arduino core 2.0.17 compile PASS, flash 1,000,057/1,966,080 bytes, static RAM 46,836/327,680. Owner supplied final OSA session b7d4a616232011f1: phase passed, disconnected, error null, elapsed 621,836 ms. Run 610251761, wakes 1/2/3, distinct boot IDs 959b6752/12cf96bb/d913c9ba, causes 0/4/4. Prior awake durations on wakes 2/3: 4,095,144 and 2,096,628 us; both prior flags 7 (local handshake, shutdown and <10 s). Three host transactions 1150/868/623 ms, connect 849/665/423 ms, challenge 125/34/23 ms, clock write/readback 45/50/45 ms. Discovery metric zero is known instrumentation omission. Host connection intervals 295.708 and 303.585 s; includes variable awake and connection latency, not direct sleep duration. Pre-sync cold clock invalid; subsequent differences +6 and -1 s; post-sync -1/0/0 s. Clock offsets vary, no fixed correction justified; PCF8563 candidate, opportunistic GPS while riding and OSA BLE time, Wi-Fi time only when already needed. Actual terminal sleep/shutdown of third wake not remotely observed. Two prior timer sleeps and awake target verified in bare USB B2, no physical sensors/TFT, logging or field power budget yet. Board charger blue LED still flashes while sleeping; exact charger and resistor identification pending. Owner wants durable OSA per-wake logs, not manual observation; current B2 results volatile, persistent logging next. Do not claim measured current or production wake performance. No firmware/server executable changes in this record.
