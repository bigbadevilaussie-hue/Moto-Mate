# Moto-Mate MkII

## V1.38 owner autonomous BENCH runtime PASS — 2026-11-27

Owner serial confirms OTA accepted 1544848 B then RTC_SW_CPU_RST and V1.38 boot, NVS loaded schema2 BENCH, no routine Wi-Fi. Three repeating normal cycles, BLE identities 6382e274 / 85aa9e6f / 36a99633, startup251/237/238 ms, verified BLE shutdown PASS all three, MPU absent quiesce, countdown5..1, pre-sleep30006465/30006797/30006797 us (cycle2 confirmed by retained prior on cycle3), timer30000ms and GPIO13 active-low EXT0 armed. First two subsequent wake/continuation sequences confirmed; third sleep entry reported. No BLE client connection evidence yet, no actual elapsed sleep measurement, no physical maintenance lease expiry test, no sensors attached at this checkpoint. Owner reports screen flicker; V1.39 fix already staged, not yet deployed. Keep FIELD disabled. OSA1.45 host regression/deployment pending. Owner plans MPU6050/SHT40 attachment after lunch. No firmware changes in evidence checkpoint.


## V1.39 — TFT flicker fix (2026-11-27)

Owner reports terrible BENCH display flicker on V1.38. Source confirms full-screen black clear and redraw every one-second tick; this is the identified software cause, physical verification pending. Display.cpp now draws static BENCH heading only on entry and updates changed formatted readings/countdown using a reusable 160x33 RGB565 strip (10560 bytes pixel storage plus overhead). Compose offscreen then push completed row; no periodic whole-screen clear. Same strip buffers sleep countdown text; released after final retained emoji or mode exit. Allocation failure logs and falls back to partial direct redraw, which may locally flicker. No refresh cadence, radio, sleep, settings or hardware/backlight GPIO changes. V1.39 staged on main, owner build/OTA/visual validation required. V1.38 owner compile PASS; exact full BLE/30/30/maintenance/OSA qualification still pending. Existing OTA verification over Wi-Fi can time out after autonomous boot; serial is required. No FIELD enable.


## V1.38 owner build PASS — 2026-11-27

Owner native wake/look regression PASS; Arduino compile PASS: 1544257/1966080 B flash (78%), static RAM 69212/327680 B (21%), remaining 258468 B. Fixed build receipt schema1, version1.38, motomate.ino.bin 1544848 B, MD5 bb0e13bd6da4704989fcae42baf49f7d, SHA256 66e602182b163799195bcdaf67738718d8901faf88df8babe1def5412c57ad05. This is build evidence only, not deployment or runtime BLE qualification. OSA1.45 suite/deployment not yet reported. Initial upload from running V1.36 maintenance via OSA; new V1.38 boot defaults to Wi-Fi-off BENCH so old OTA verifier may report timeout despite successful commit; verify actual serial boot/version and do not blindly retry. Backlight GPIO4 still held LOW in this build; module photograph suggests onboard transistor but electrical qualification and firmware control remain separate. No FIELD activation.


## V1.38 / OSA 1.45 / GUI 2.05 — normal BENCH BLE slice (staged 2026-11-27)

Owner directed continue coding, donor BleManager available if stuck. Earlier B1 V1.07 and B2 V1.08 real S3/OSA handshake, clock, disconnect, shutdown and timer wake tests PASS; archived source at 52b2cec BenchSleep.cpp. Restore same Arduino core 2.0.17 BLE init/server/advertise/stop/deinit(false) and disconnect 250ms restart ordering into BenchBleTelemetry.cpp, not a new stack, no changes to protected donor. New distinct read-only service 0da90001-7d91-4a75-9a7c-56d1f29b0731, name Moto-Mate, identity 0da90002 ASCII MM/1/<revision>/<8hex bootid>, 0da90003 20-byte LE snapshot <BBBBIIHhHH: schema1, validity bits rail/temp/RH, cause, reserved0, cycle, uptime ms, rail mV, signed temp centi-C, RH centi-percent, prior awake seconds. Fresh cached sensors only, 500ms update; invalid fields zero with validity clear, rail estimate explicitly NOT qualified battery voltage. No writes, clock sync, command, production log transfer or pairing yet. Normal awake BLE only, stop by final 5s and verify both stacks off before sleep; skip BLE if awake budget too short or explicit legacy sleep logging test. Physical long press stops BLE before existing Wi-Fi manager 800ms guard; failed shutdown refuses Wi-Fi and resets, no automatic renewal. V1.37 timing/maintenance base staged but not hardware tested; V1.38 supersedes it, owner first actual build/OTA. No routine Wi-Fi, FIELD disabled.

OSA1.45 existing Bike-Mate broker scans Moto only after missed Bike scan, B1/B2 queued tests have priority, never interrupts established Bike connection or creates second scanner. Read and disconnect, validate identity/packet/ranges, cache distinct snapshots, age monotonic, alive <=75s (provisional normal 60s cycle freshness, NOT connection proof). GET /moto/ble/status and existing /moto/status data. GUI2.05 displays fresh rail estimate with * uncalibrated label, temp/RH or dashes, clears stale; raw data via OSA only. History and storage not invented. Existing GUI maintenance controls not yet final physical lease-aware. No OSA or GUI deployment performed. Source tests added (decoder, routes); native/Arduino/Python execution on owner's Mac pending. Owner needs build, OSA suite before cutover, OTA from existing V1.36, serial boot because old Wi-Fi verifier cannot see automatic Wi-Fi-off new firmware. Test 30/30, actual GUI updates, OSA absent, physical entry, BLE shutdown and 180s expiry; strict stalled HTTP lease remains unqualified. Full radio port/clock/log/OTA verification redesign later. MAIN ONLY no branches.


## V1.37 — owner-priority normal BENCH loop (2026-11-27)

Supersedes previous V1.37 shadow-only gesture plan. Default healthy BENCH boot/reset/timer wake runs real configured awake/sleep (defaults 30000/30000 ms) without arming bench_sleep. Awake includes boot; live uncalibrated protected-rail estimate, fresh SHT temperature/humidity or --, remaining seconds, then last five seconds sleepy countdown and retained static emoji across actual deep sleep. Existing safe GPIO holds, verified MPU quiesce, timer wake and released GPIO13 EXT0 used. Held/late button at sleep boundary falls back to timer-only to avoid immediate wake loop; no auto Wi-Fi on faults. Failed safe sleep arm or MPU quiesce enters radio-free 30s retry, not a proven low-current state. TFT init failure does not prevent sleep. Explicit legacy sleep log arm still consumed once, max three qualified timer-wake samples, normal cycle no routine file writes; no change to locked schema or identity. FIELD still disabled, recovery boot no normal display or automatic Wi-Fi.

Physical GPIO13 long hold (NVS default 2000 ms from sampled stable press, release/repress after cold boot held) is sole BENCH maintenance admission. Fixed 180s monotonic lease begins before storage mount or Wi-Fi. Existing Wi-Fi manager unchanged, no remote entry, no renewal, reconnect/retry bounded by same lease. At expiry stop HTTP/abort unfinished OTA if needed then software reset to autonomous BENCH; reset/OTA never creates new lease. Existing OTA endpoint checks lease for authenticated operations, status, upload chunks and commit; new OTA requires >30s remaining, chunks and final commit >2s remaining. Existing synchronous Arduino WebServer request processing and flash operations may delay observed expiry; hard real-time timeout during a stalled request NOT yet qualified, no claim of full production lease. Existing OSA Wi-Fi OTA verifier cannot automatically see the new autonomous boot; initial V1.36->V1.37 upload via OSA accepted, verify actual version and cycle by serial, do not retry solely for verification timeout. Future OSA verify via BLE or fresh physical session required.

BLE only requested for normal awake, but protected donor BleManager is NOT present in Moto-Mate; currently both radios off during normal cycle. No fake BLE, no GUI live telemetry, no protected radio sequence changes or donor edits. BLE/OSA normal telemetry port is next priority, with proper guard/quiesce and physical tests. Single/double gestures recognised/logged, no RR/BatteryTest actuation yet. Config-active flag remains false for whole schema because only timing fields activated. No new settings write endpoint. Native test source extended for configurable 2s/400ms gesture timings; local execution/Arduino compile and physical cycle/lease evidence pending. Main only, no branch. Owner to build, upload from V1.36 existing maintenance, observe two complete default cycles, then hold GPIO13 ~3s and observe Wi-Fi connection, 180s expiry, autonomous cycle return. Measure actual sleep time externally and check TFT. No deployment by assistant.


## V1.36 owner reset persistence PASS — 2026-11-27

After owner reset, authenticated settings readback reports nvs_ok=true, nvs_state=loaded, storage_schema=2, config_active=false, effective BENCH, all 41 stored values unchanged from migrated readback (37 new numeric fields and four legacy flags). Native regression, Arduino build, OSA OTA/alternate slot verification, migration and reset persistence PASS. V1.36 qualified for tested BENCH configuration foundation only; actual power-loss recovery, factory reset, new-field editing and runtime consumers still pending. FIELD remains disabled. Next V1.37 configurable wake gesture integration. Main-only workflow; redundant v1.36-settings branch previously fast-forwarded and now eligible for owner deletion; connector has no branch-delete action. No firmware changes.


## V1.36 owner OTA and NVS migration PASS — 2026-11-27

Owner verified OSA job af715bb4b89b563b phase=passed, error=null, device/build version=1.36, verified=true, new boot/slot/revision verified; MD5 1e14b8e9074a2d21e761f1d0bba7f735 and SHA256 52fd4fc8d2c442b23117b940b58a219f7f4ed7acc7afb1b98ff4897b58cb9c23 match build receipt. Authenticated GET /settings: profile/effective BENCH, bench_sleep=false, bench_sleep_log=false, verbose=false, field_ready=false, nvs_ok=true, nvs_state=migrated, storage_schema=2, config_active=false; all 37 numeric fields returned with expected defaults, ADC calibration unqualified. Runtime boot reports image954192 B, LittleFS mounted 8192/131072 B used, log identity valid, absent external RTC handled, Wi-Fi 1041 ms at 8.5 dBm RSSI -39, TFT init, wake shadow reaches sleep_pending as expected. No actual sleep or new setting actuation in shadow. Correct OSA CLI connect/ota POST requires EMPTY body, not JSON {}; initial 403 was rejected before upload. Successful OTA uses existing BENCH commissioning service. Next owner reset and readback must show nvs_state=loaded with identical settings; physical persistence and subsequent configurable runtime integration still pending. No firmware changes or FIELD enable.


## V1.36 owner build evidence — 2026-11-27

Owner pulled main and ran native settings regression: PASS (37 fields, 160 bytes, migration codec, validation, corruption, readiness). Arduino build PASS: 953593/1966080 B flash (48%), 48224/327680 B static RAM (14%), 279456 B remaining. Fixed build receipt schema1, motomate.ino.bin V1.36, 954192 B, MD5 1e14b8e9074a2d21e761f1d0bba7f735, SHA256 52fd4fc8d2c442b23117b940b58a219f7f4ed7acc7afb1b98ff4897b58cb9c23. OTA not yet reported. Next: confirm existing BENCH settings healthy and no bench sleep/log arm; owner performs OTA using existing OSA/GUI commissioning path, observe V1.36 boot and NVS migrated schema=2, read settings and reset to verify loaded/unchanged. No physical migration/persistence PASS yet, no FIELD enable. Temporary v1.36-settings branch already integrated; delete after owner test. No firmware change.


## Locked owner Git rule — 2026-11-27

Work directly on main. Never create branches. Test changes and keep commits scoped; owner controls physical validation and deployment. V1.36 was already fast-forwarded to main at 2feb858 before owner hardware testing; the temporary v1.36-settings branch is redundant and should be deleted after the owner test, not merged again or used for further work. No firmware changes in this documentation checkpoint.


## V1.36 — NVS configuration foundation (staged 2026-11-27)

Adds 37 integer configuration fields alongside the four legacy profile/commissioning flags (41 stored values in total). SettingsPolicy.h is the normative ordered defaults/range registry; explicit 160-byte little-endian record with schema marker, legacy flag word and CRC32. Namespace motomate, key config_v2. On absent v2 only, migrate valid config_v1 flags or initialise defaults on genuinely missing legacy; verify commit by reopen/readback, never erase legacy or touch mmlog identity. Existing corrupt/unreadable v2 never falls back to v1. Unknown schema and I/O failures report unhealthy; identical validated saves avoid flash wear. Legacy GET/POST contract remains schema=1 at the wire envelope; GET adds storage_schema=2, config_active=false and config object; legacy POST preserves new fields and retains former one-shot logging disarm semantics. New parameters storage-only/read-only via existing HTTP until physical maintenance gate and runtime consumers are integrated. Calibration status defaults unqualified, signed offset and gain ppm persisted. No generic calibration toggle or new write/reset endpoint yet. Parked MPU/SHT cadence follows wake policy, not a separate tunable; 5 s ride/24 h collection remain locked logging design pending reconciliation. 180 s lease and 10 s FIELD ordinary awake ceiling not settings. Range limits are lead-proposed and voltage thresholds provisional. The retained config_v1 word is a migration snapshot, NOT synchronised with subsequent v2 edits: downgrade to <=1.35 needs controlled settings/recovery review, especially one-shot flags; no automatic downgrade. Existing BENCH commissioning still starts Wi-Fi on reset and V1.35 shadow gesture timings remain unchanged; not yet production physical maintenance policy. No FIELD enable, radio sequence, logging schema, sensor, actual timing or display changes.

Native C++11 regression PASS in development container with -Wall -Wextra -Werror -pedantic, exact committed header/test blobs c07358f61fe550d62e13607dc4f43356b19c7da3 / 8fdc3572101e4af47634028b1fdda8f97077edb9 verified. Coverage: all legacy flag combinations, 37 fields outside both range boundaries, cross-field relations, signed offset and zero boolean roundtrip, all 1280 individual bit flips, truncated lengths, valid CRC with invalid range or unknown schema, known CRC32 fixture. This does not test real NVS or HTTP/Arduino build. Next owner: pull, run native test and fixed build wrapper; report compile/size. Before OTA check existing settings and no bench sleep/log arm; owner-controlled upload via existing BENCH commissioning OSA route, observe [BOOT] MOTO-MATE V1.36 and [NVS] settings=migrated schema=2 requested=BENCH, then GET settings, reset and verify loaded/unchanged config and NVS status. Actual migration, persistence, legacy OSA/GUI regression, power-loss/recovery and factory-reset workflow pending. Next stage gesture timing integration from NVS, then real coordinator and physically gated maintenance. Do not mark full stage 5 closed on native codec pass.


## Current owner direction — 2026-11-27

This section supersedes conflicting historical design text below; it describes requirements, not implemented or tested behaviour.

- Technical lead owns routine coding, naming, defaults, units, validation, configuration schema and implementation choices. Owner directs requirements and may intervene. Maintain the handover, operating design, settings design, port plan and README as work progresses. Preserve protected radio approval boundaries, locked logging contract, owner-controlled physical tests/deployment and no FIELD activation.
- Physical GPIO13 long press is the ONLY maintenance entry. No GUI, OSA, BLE, Wi-Fi or queued remote entry; remove activation UI and reject remote entry at the authority boundary. GUI may attach to an existing device-authorised lease. OTA admission requires that lease. Absolute 180 s monotonic deadline, no renewal or reboot reset. BENCH commissioning bypasses are historical test behaviour, not production authorisation.
- Single click Rider Ready 30 s; double click Battery Test 10 s; long press maintenance, proposed default 2 s (V1.35 shadow uses 1.5 s); double window 350 ms. One debouncer (30 ms), nonblocking mutually exclusive events, no lease extension, stuck input protection. Real boot capture and physical qualification remain open.
- Same firmware BENCH/FIELD NVS profile. Configurable BENCH awake/sleep defaults 30/30 s; FIELD sleep default 300 s now configurable (supersedes fixed-sleep NVS prohibition); ordinary FIELD awake ceiling 10 s, sleep sooner. No civil-time scheduling. Changing FIELD cadence requires explicit reconciliation with locked schema-1 monitor cadence before enabling; do not silently change wire semantics.
- FIELD screen never on per latest owner wording; whether deliberate physical button screens and PANIC are exceptions remains unresolved. BENCH normal voltage/temp instrumentation then sleeping emoji. Preserve sacred cold-start cues subject to resolving display policy.
- Maintenance temporarily suspends normal battery-first operating policy for human intervention; safe electrical/flash limits and lease deadline still apply. Engine start/stop, crank sag discrimination and auxiliary rail safety require qualified evidence.
- No firmware changes in this documentation checkpoint. V1.35 wake policy staged/shadow only; V1.34 last confirmed firmware; OSA1.44 deployed with 59/59 tests, GUI2.04 icon visually confirmed; standalone bundle and full lifecycle remain unqualified.


**Start here:** [Current AI handover](aihandover.md) (2026-11-27), [operating design](operating-design.md), [GUI port](gui-port.md). Firmware1.34, live OSA1.43, GUI2.02 visually owner-verified; staged OSA1.44/GUI2.03 awaiting regression and owner-controlled deployment. Latest checkpoint supersedes older commissioning text below. Use tools/moto_build.py for fixed binary and revision/hash receipt; OSA owns OTA, GUI is terminal. No FIELD activation.

## Owner rule: Moto-Mate governs OSA

**OWNER RULE — 2026-11-26: Moto-Mate dictates how OSA works from now on.** The owner explicitly authorises the technical lead to fix and rework OSA to meet Moto-Mate's requirements, rather than constrain the new firmware to OSA's historical architecture. This supersedes the earlier requirement for separate scope approval for OSA engineering changes. It does not authorise arbitrary changes to Bike-Mate firmware, protected ESP32 radio sequences, or other device firmware. Preserve the battery-first prime directive, the ESP32's autonomy, and OSA's role as sole BLE broker. Preserve working services and user-facing behaviour for Bike-Mate, Fan-Mate and Audio-Mate, especially Alexa telling the owner to check the bike, existing alerts, storage and OTA workflows. Compatibility is a behavioural requirement, not a mandate to retain flawed internals. Do not silently remove features or change alert routing. No automatic deployment: use small testable changes, regression tests and an owner-controlled cutover with rollback.


**Battery and charging-system diagnostics for a motorcycle — without becoming the biggest drain on the battery.**

## Project Prime Directive: SAVE THE BATTERY AT ALL COSTS

Protect the battery first. Collect all useful data needed to diagnose the battery and charging system, then reliably preserve and deliver that data. Convenience, display uptime and connectivity are secondary.

- Target parked deep-sleep current: **<= 1 mA at the AGM battery input**, not merely at the ESP32. This is a design target, not a verified Moto-Mate measurement.
- Account for wake-up energy, radio activity, retries, regulators and peripheral leakage as well as sleep current.
- Collect voltage, temperature, timestamps, resting discharge trends, cranking sag, charging voltage and engine/ride transitions where the hardware and sampling permit.
- Preserve data across sleep and failed transfers. Avoid indefinite awake states and unnecessary loads. Exact protection thresholds and sampling changes remain to be agreed.
- Voltage telemetry is not a direct measurement of battery current, capacity or state of health.

## Major project rule: clean firmware port, not blind reuse

Owner direction (2026-11-26): this is the opportunity to remove accumulated firmware technical debt. Bike-Mate is the functional reference, not a line-count or implementation constraint. The previous approximately 95% reuse target is no longer a firmware acceptance criterion. Reuse clean, proven implementations; replace unnecessarily complicated, blocking, duplicated or obsolete implementations where justified by the audit and verified against the required behaviour. Do not copy bad code merely to preserve reuse. Preserve functional and host/data compatibility, battery-first policy, core 2.0.17, and the explicitly protected BLE/Wi-Fi implementation and proven recovery sequence. Protected changes still require separate owner approval. GUI remains essentially a straight copy. This is not permission to discard live features, rewrite proven code for style, or modify the donor firmware without scope approval. OSA engineering is authorised by the owner rule above.

Moto-Mate is the successor to [Bike-Mate](https://github.com/bigbadevilaussie-hue/Bike-Mate), which was built, installed on the bike and worked. Moto-Mate changes the hardware and rebuilds the coordinator while retaining proven functionality and communication contracts.

The historical reuse target was approximately 95%; firmware quality and functional compatibility now take precedence over source reuse. The GUI is essentially a straight copy of Bike-Mate's Python/Tk application. Adapt GPIOs, display, sensors, power management and other genuinely hardware-dependent behaviour as required. Preserve existing protocols, file formats, functionality and proven workarounds. Clean and optimise the ESP32 code as it comes over: remove verified dead code, consolidate duplicates and fix evidenced faults using [TODO.md](TODO.md). No gratuitous refactoring or unauthorised changes to protected BLE/Wi-Fi code.

The reviewed donor is Bike-Mate firmware V6.49 (commit [bc5766c](https://github.com/bigbadevilaussie-hue/Bike-Mate/commit/bc5766c319da25622db1f2c86b58339afd07d5ad)); GUI v4.52 is the supplied visual reference. Check current Git HEAD before porting.

## System architecture

**Current design authority:** [operating-design.md](operating-design.md) records the owner-approved fresh coordinator, wake-first decision process, work priorities, maintenance session workflow, GUI/OSA responsibilities, predictive diagnostics and remaining decisions. Read it before porting. Older conflicting descriptions are historical.

Moto-Mate has three components:

```text
Moto-Mate ESP32-S3 <-- BLE --> OSA Server <-- HTTP --> Moto-Mate GUI
                                  |
                             Local logs / GitHub
```

- Firmware: wake/sleep, voltage and sensor sampling, engine-state detection, ride and wake logs, BLE telemetry, file transfers, and Wi-Fi maintenance/OTA.
- [OSA Server](https://github.com/bigbadevilaussie-hue/OSA-Server): mandatory iMac-based Python/Bleak central, port 9999, clock synchronisation, telemetry cache, command handling, log reassembly, storage, notifications and GitHub uploads. OSA owns the BLE link; the GUI reads OSA's HTTP cache.
- GUI: preserve Bike-Mate's status display, voltage and temperature graphs, reports, settings, maintenance and OTA functionality.

OSA is currently V1.33 at the reviewed baseline. Whether Moto-Mate replaces Bike-Mate in OSA or runs alongside it is still open. Do not invent new device identities, endpoints or storage paths prematurely.

## Hardware (current schematic design)

| Component | Design |
| --- | --- |
| MCU | ESP32-S3 Super Mini (HW-747) |
| Display | 1.8-inch 128 x 160 ST7735 colour TFT; direct active-high BLK |
| Environmental sensor | SHT40 temperature/humidity |
| Motion sensor | MPU-6050 gyro/accelerometer, always-on 3.3 V with sleep mode |
| GPS | NEO-6M, switched 5 V through 10 uH / 100 nF / 470 uF filter |
| Supply | 12 V AGM, two MP1584EN buck converters |
| Switched rail | IRF4905 P-channel high-side switch, NPN driver |
| Battery ADC | 470k / 100k divider, measures protected rail after series diode |
| Other | Active buzzer, red/green LEDs, wake button, kill switch, 30 mm fan, dual USB-A charger |
| Enclosure | V4 OpenSCAD design |

The latest committed schematic/layout is [moto14wires30cuts.json](moto14wires30cuts.json): **24 x 56 holes, 14 jumper wires, 30 track cuts, 35 nets**. It is the current drawn circuit, not a physical test or fabrication approval. [veroboard.md](veroboard.md) and [project.md](project.md) contain older descriptions and must be reconciled against it.

The optional external 32.768 kHz RTC crystal and two 15 pF capacitors remain in the committed layout. Omitting them to avoid soldering the underside ESP32 pads has been discussed; the layout has not yet been changed. Existing internal RTC and OSA clock synchronisation are the intended fallback if omitted.

Open hardware review items: AGM connector footprint placement, input diode current rating relative to fuse and USB load, transient protection and bulk capacitor voltage margin, TFT BLK current, GPS back-powering, C2 ADC capacitor (100 nF in JSON vs 1 uF in older requirements), and measured sleep/wake current. Confirm actual wiring and component ratings before fabrication or power-up.

Ordered components, physical stripboard confirmation, TFT and enclosure details, and unresolved component/fit checks are recorded in [hardware-inventory.md](hardware-inventory.md). This inventory is not a final BOM.

### GPIO map

Source: [Config.h](Config.h). ESP-side signal names.

| GPIO | Function |
| --- | --- |
| 1 | Battery voltage ADC |
| 2 | Buzzer |
| 3 | Red LED |
| 4 | TFT backlight |
| 5 | 12 V MOSFET control / green LED |
| 6 / 7 | I2C SDA / SCL |
| 8 / 9 / 10 / 11 / 12 | TFT RST / DC / CS / MOSI / SCLK |
| 13 | Wake button |
| 44 / 43 | GPS RX / TX (UART1; ESP TX via 1k resistor in schematic) |

## Intended operation

Parked five-minute wake interval, deep sleep between wakes, riding mode, rider-ready and battery-check modes, LittleFS wake/ride logs, elapsed 24-hour gzip/upload routine (no 04:00 trigger), BLE telemetry and Wi-Fi-only maintenance/OTA. These are intended features, not claims that the Moto-Mate firmware already implements them. Reconcile older written timing requirements with actual donor code before changing state-machine behaviour.

## Build status

**V1.06 — targeted flash diagnostic crash fix; compile/runtime pending.** V1.05 compiled (981357 B flash, 46828 B static RAM) and uploaded with esptool 5.6.0, boot/sprite/splash passed, but repeatable LoadProhibited at EXCVADDR 0x8. Owner decoded matching ELF f9135b5542ae49f3: EspClass::getFlashChipMode() Esp.cpp:356 -> commissionStorage() BenchCommission.cpp:175 -> loop() -> loopTask. Removed mode query only; retained size and speed diagnostics, partitions, LittleFS, Wi-Fi and clock unchanged. Verify [BOOT] MOTO-MATE V1.06 then [FLASH], [OTA], [PART], [FS] progress. V1.05 never reached storage mount or radio tests.

**V1.05 — USB commissioning implementation (compile/upload PASS; runtime FAIL as above).** Serial banner, hardware/OTA/LittleFS diagnostics, erased-only filesystem provisioning, DHCP Wi-Fi bench test and Opal LAN/NTP clock test, Wi-Fi shutdown. BLE/OSA explicitly pending (protected manager dependencies); no physical TFT, GPIO outputs, field sleep or sensor tests. Wi-Fi bench requests 20 dBm ceiling; no production TX change. V1.04 owner compile/upload and full runtime PASS: 342389 B flash, 19888 B static RAM, zero overruns on complete capture (max gap 8005 us, max service 6680 us, deadline 10000137 us). Earlier partial run had one 100 ms overrun. Use incremental compile (no --clean), upload with esptool 5.6.0 override `upload.pattern`, CoolTerm 115200 USB CDC; disconnect it before upload. No credentials in Git.

**Latest baseline: V1.03 — owner compile/link PASS (2026-11-26).** Commit [b2d36d8](https://github.com/bigbadevilaussie-hue/Moto-Mate/commit/b2d36d88605a7b6ffeae97e979727b6713cb3b35): 386841 B flash, 20528 B static RAM, 307152 B remaining. Splash framework with seven modes and dynamic version; off-screen test only, final artwork and runtime pending. See [assets/display/README.md](assets/display/README.md). The V1.02 baseline below is retained for comparison.


**V1.02 — pre-port display smoke test; compile and link PASS (2026-11-25).**

- Firmware commit [29808f4](https://github.com/bigbadevilaussie-hue/Moto-Mate/commit/29808f474c3a8f89cff91e7fac68e57d191e0534); owner confirmed local fast-forward and successful Arduino CLI compile. V1.01 added LovyanGFX includes ([8648189](https://github.com/bigbadevilaussie-hue/Moto-Mate/commit/8648189717608e39f3aaeda9fc4b60f023bd6132)); V1.02 exercises actual display and sprite APIs.
- Arduino ESP32 core **2.0.17 ONLY**; LovyanGFX **1.2.36** installed and compile-verified. Preserve this known build combination; no unsolicited upgrades. Adafruit_GFX / Adafruit_ST7735 remain included for now.
- Board: ESP32S3 Dev Module, FQBN esp32:esp32:esp32s3. The owner provided board details confirming all compile options below. 4 MB flash, disabled PSRAM and min_spiffs are provisional smoke-test settings, not confirmed physical memory specifications or final storage partition. min_spiffs supports OTA but has only about 190 KB filesystem space; revisit against logging requirements and actual flash capacity.
- Original V1.00 include-only test passed at approximately 300 KB flash / 19.2 KB RAM; retain as historical baseline, not a controlled library-size benchmark.

Reproduce on the owner's iMac (install exact library version only if needed):

```bash
cd ~/Documents/Arduino/motomate && arduino-cli lib install LovyanGFX@1.2.36
cd ~/Documents/Arduino/motomate && arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=min_spiffs,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled .
```

Actual owner compile output:

```text
Sketch uses 383889 bytes (19%) of program storage space. Maximum is 1966080 bytes.
Global variables use 20528 bytes (6%) of dynamic memory, leaving 307152 bytes for local variables. Maximum is 327680 bytes.
```

The 160 x 128 RGB565 sprite needs 40,960 bytes (40 KiB) of pixel storage plus overhead at runtime; this is not included in static RAM. Available contiguous heap and full firmware memory use remain unmeasured.

### Display direction and smoke test

The owner wants high-quality colour motorcycle instrumentation, not an OLED-style monochrome port. LovyanGFX is the preferred candidate for the TFT implementation following the successful build; final runtime acceptance remains pending. Suggested graphics include readable digital speed and charging voltage, colour-coded battery gauges/warnings, countdown and OTA progress, and a future artificial horizon/lean-angle view using the MPU6050. These are design ideas, not implemented or approved final screens. Dynamic lean estimation requires calibration and sensor fusion; accelerometer gravity alone is not a reliable motorcycle lean-angle measurement during cornering. Keep riding graphics subordinate to battery preservation; avoid unnecessary parked backlight/display activity.

V1.02 configures lgfx::Panel_ST7735S and Bus_SPI (SPI2_HOST, mode 0, provisional 16 MHz, write-only, no MISO; GPIOs from Config.h). The panel is 128 x 160 portrait; the RAM sprite is 160 x 128 landscape. No physical TFT init, rotation, sprite transfer or backlight control is performed. The temporary sketch draws a synthetic horizon, circle, text and dummy 14.2 V value, checks allocation, dimensions and two known pixels, frees the sprite, then prints PASS/FAIL. It does not test live voltage, MPU, GPS, SPI traffic or physical display. The temporary serial boot delay and idle loop are not production power management.

When an ESP32 is available, run the test even with no TFT attached. Expected output at 115200 baud (not yet observed):

```text
[BOOT] MOTO-MATE V1.02
[TFT] PASS: 160x128 RGB565 sprite and pixel readback
```

Next hardware test: validate panel offsets, colour order, inversion, rotation, actual SPI rate and backlight current; then test physical display and sleep/backlight shutdown. Full firmware/GUI port and OSA integration remain unstarted. No Moto-Mate runtime, physical current, wiring or fabrication validation has been reported. The 47 donor-audit tickets remain open; smoke-test code is not the production DisplayManager.

## Non-negotiable engineering rules

- **Arduino ESP32 Core 2.0.17 ONLY.** Locked due to macOS toolchain and NimBLE/BLE compatibility drama. No 3.x migration, BLE stack replacement or unsolicited dependency upgrades.
- **BleManager.cpp / BleManager.h are protected.** Weeks of work went into them. Preserve GATT UUIDs, timing, packet framing, callbacks, handshake, ACK handling, retry behaviour and file-transfer semantics. Discuss any demonstrably necessary hardware adaptation with the owner before touching this code.
- **Wi-Fi / BLE coexistence:** never run both at once. Stop BLE, wait 500 ms, then start Wi-Fi. Preserve the donor's proven IDF teardown/reinitialisation sequence (72/72 on Opal). Keep the 8.5 dBm TX power baseline; validate on the S3.
- [codeprt.md](codeprt.md) documents GPIO holds, TFT polarity/sleep, MPU sleep and other requirements. Resolve contradictory hardware statements with the schematic and actual circuit, rather than assuming old prose is correct.
- Never commit or disclose Config.local.h or credentials. Verify ignore rules before creating local config; do not assume it is already ignored.

## Rules

Non-negotiable. Read first.

### Moto-Mate-specific precedence

The original owner rules below are retained verbatim. Apply the following later explicit owner decisions where they conflict:

- The latest clean-firmware-port direction above supersedes the earlier firmware reuse percentage and any interpretation that requires copying defective implementations. The protected-code and test requirements remain unchanged.

- ChatGPT is technical lead and performs requested patching and porting directly through GitHub. The owner directs scope, approves protected changes and performs physical/local testing. Do not assume permission for unrelated edits or deployment. The original rule about the owner editing files himself is superseded only for requested GitHub work.
- Read current GitHub source immediately before a patch instead of asking the owner to paste a file that is available through GitHub. If the relevant local contents differ or cannot be inspected, request current contents; never guess.
- Every terminal command supplied to the owner starts with cd. Verified repository: /Users/nick/Documents/Arduino/motomate. The original shell examples below are historical quotations, not commands to paste unchanged.
- Use /tmp Python scripts for local patches. The original python3 -c example is a read-only diagnostic, not a patch; prefer a script to avoid ambiguity.
- Bike-Mate requires :PartitionScheme=min_spiffs. Confirm the Moto-Mate S3 board/partition configuration rather than assuming the donor FQBN is correct. Arduino ESP32 Core 2.0.17 ONLY.
- Firmware patches require a version bump and verification against [BOOT] MOTO-MATE VX.YZ; documentation-only changes do not bump firmware. The donor boot marker remains [BOOT] BIKE-MATE VX.YZ.
- For "fully commit" / "complete the commit", use the requested full commit workflow on the local repository when authorised. Direct GitHub commits do not imply access to or inclusion of unpushed local changes. Never commit secrets or Config.local.h; check ignore rules before staging everything. Tag a mentioned release version as instructed.
- Clean and optimise the ESP32 direct port using [TODO.md](TODO.md). Preserve protected BLE and proven Wi-Fi sequence; no speculative changes.

### Tone

- Be direct. No lectures. No explaining basic git concepts.
- Talk like a competent senior dev, not a tutorial.
- If you're about to explain something obvious, shut up and give the command.
- No time-of-day references. No sleep/rest/break suggestions.
- Nick decides when to work and when to stop.

### Commits

- "fully commit" / "complete the commit" = `git add -A`, commit, push to origin, tag if a version was mentioned.
- Never say "working tree clean" unless asked for status.
- Never ask to re-check output already pasted.

### Shell - zsh

- Multi-line pastes break. `#` triggers history expansion.
- **Write patches as Python scripts to `/tmp/` and run with `python3 /tmp/patch.py`.** Never paste heredocs directly into the shell.
- Every patch script must exit non-zero on anchor miss. Silent no-ops are worse than crashes.
- Bike-Mate compile requires `:PartitionScheme=min_spiffs`.
- zsh bracketed-paste mangles multi-line pastes even inside heredocs - lines at the boundary get eaten silently. Fix once: `echo 'unset zle_bracketed_paste' >> ~/.zshrc` then `exec zsh`. Or run `bash` for the session.
- If a heredoc still misbehaves: write the script with `printf '%s\n' \` and one `'line' \` per line, redirect to `/tmp/x.py`, then `python3 /tmp/x.py`.
- Never paste Python with `python3 -c`. Always `/tmp/x.py`.
- A Python string anchor matches the exact bytes. If a file ends without a trailing newline, `"...;\n"` will MISS. Check with `python3 -c 'print(repr(open("f").read()[-40:]))'` before writing the anchor.

### Patch discipline

- **Never patch a file you haven't seen the current contents of.** If the last paste was more than one exchange ago, ask again.
- **Never guess anchors or regex.** Read the file. Grep for the exact line. If the anchor misses, stop and ask for current contents. Do not guess.
- **One patch. One file when possible. Test before the next.**
- **Version bump is part of every patch.** `[BOOT] BIKE-MATE VX.YZ` is the ground truth for which binary is on the board. Never trust a paste that disagrees with the log.
- **After every patch, state how to verify it worked.** A specific log line, OLED state, or endpoint response. "Compile and flash" is not verification.

### Diagnosing

- **Never patch a theory.** Propose it, propose the cheapest way to confirm, get the answer, then patch.
- **Git history is the answer more often than the code.** When something regressed, `git log --oneline -- <file>` and `git log -L` show when.
- **If a subsystem has a comment like "proven 72/72" or "two days and four AIs", do not touch it on a theory.** Ask why it's that shape first.
- **Intermittent != broken.** The fix is usually a retry, not a rewrite.

### Communication

- Answer, then stop. Do not explain three things at once.
- Do not write nine scripts at once. One file, one patch, one command.
- Nick edits files himself. Ask before assuming automation.
- "Cages" = cars.

### Session hygiene

- This is one stateful session, not isolated replies.
- Before proposing a fix, ask: "have I confirmed current state, or am I assuming it?"
- Before saying "this is a bug", ask: "could this be load-bearing for a reason I don't see?"
- Read all pasted output. The answer is usually in the line you skimmed.

## Project documents

| File | Purpose |
| --- | --- |
| [aihandover.md](aihandover.md) | Current project direction, repository references, technical lead responsibilities and new-chat handover |
| [TODO.md](TODO.md) | Full ESP32 audit and prioritised porting backlog |
| [port-plan.md](port-plan.md) | Dependency-ordered implementation candidates and acceptance gates |
| [operating-design.md](operating-design.md) | Current operating requirements, decision record, GUI/OSA findings and predictive diagnostics |
| [Config.h](Config.h) | GPIO assignments and firmware version |
| [motomate.ino](motomate.ino) | Fresh V1.04 setup/loop coordinator |
| [runtime-framework.md](runtime-framework.md) | Coordinator contract, bench tests and integration rules |
| [hardware-inventory.md](hardware-inventory.md) | Ordered components, physical board size, enclosure/TFT fit and open hardware checks |
| [moto14wires30cuts.json](moto14wires30cuts.json) | Latest committed schematic and stripboard layout |
| [veroboard.md](veroboard.md) | Wiring guide and BOM (requires reconciliation) |
| [project.md](project.md) | Hardware and operational specification (older software direction superseded) |
| [codeprt.md](codeprt.md) | Mandatory code requirements, subject to hardware reconciliation |
| [motomate_case.scad](motomate_case.scad) | V4 enclosure |

**New AI chat:** Read aihandover.md first, then check current repository state. The battery-first directive, direct-port rule and protected software requirements govern all work.

## V1.07 BLE commissioning

See [B1 commissioning](ble-commissioning.md). Compile and physical tests pending; V1.06 remains verified baseline.

## V1.08 sleep commissioning

See [B2 sleep test](sleep-commissioning.md). Dedicated USB-only build, real timer sleep, terminal deep sleep after three wakes. Compile and physical validation pending.

## V1.09 USB power bench

[30s awake / 30s deep sleep](power-bench.md), repeating, radios off. Compile and measurements pending. Not field mode.

## V1.10 OTA maintenance bench

See [OTA commissioning](ota-commissioning.md). First USB flash then wireless slot-switch validation; local key setup required. No production sleep.


**V1.10 OTA commissioning owner PASS:** app0 -> app1, distinct boot IDs, same version and Wi-Fi restored. See [ota-commissioning.md](ota-commissioning.md). Buck-powered operation and interrupted-update recovery pending.


**V1.11 application foundation:** commissioning archived; OTA/Wi-Fi services extracted; real safe idle GPIO, button observation and ADC pin measurement. See [application-foundation.md](application-foundation.md). Compile, OTA and physical checks pending. No field operation.


**V1.12:** Owner rule BENCH/FIELD is a persistent NVS profile, not an OTA variant. Settings foundation and authenticated read/write implemented; FIELD and bench sleep capability gates remain false pending real runtime. See [settings-design.md](settings-design.md). Compile and persistence verification pending.


**AGREED owner rule (2026-11-26): fixed parked cadence, 300 seconds actual deep sleep and at most 10 seconds awake, returning sooner when work is complete. No peak/off-peak, time-of-day, timezone or DST-dependent wake intervals. UTC timestamps are evidence; elapsed-time scheduling is independent of civil time. Retire the legacy 04:00 wall-clock collection trigger; retain elevated 24-hour collection as elapsed-time due work, retry pending delivery on eligible wakes, mark completion only after verified acknowledgement. Ride, explicit maintenance, protection and safe transaction completion remain distinct exceptions. No adjustable parked cadence in NVS.**


**V1.13 physical TFT BENCH:** actual ST7735S initialization, RGB/white-border visual check, then selected artwork splash with dynamic mode/version; GPIO4 backlight remains off/unqualified. No Wi-Fi/OTA or FIELD changes. Build and physical confirmation pending. See [assets/display/README.md](assets/display/README.md).


**Current checkpoint — 2026-11-26: V1.16 owner BENCH runtime confirmed.** V1.13 compiled 817849 B flash / 46720 B static RAM; verified different-version OTA V1.10 app1 to V1.13 app0 and NVS initialisation. Physical TFT wiring corrected; V1.14 RGB/BGR correction accepted, logo visible. V1.15 typography owner-tested; V1.16 animated splash accepted, serial confirms two complete startups, temporary sprite released, NVS loaded, Wi-Fi connected in 1037/1071 ms and OTA ready, maintenance mode rendered. Reason for second boot not established. BL remains disconnected/unqualified; panel stays illuminated on bench. No measured heap low-water, full-system current, physical button wake, sensor or field validation. Current physical TFT is on the USB breadboard; reconnect momentary switch after rewiring (GPIO13 to GND).

**V1.17 port-plan stages 2/3, first hardware slice:** ADAPT donor safe GPIO hold recovery and early button sampling to S3; retain single input owner, replace donor blocking release wait with monotonic 30 ms stable-release qualification. Prepare low outputs before releasing GPIO holds, restore RTC wake pin to digital mode on EXT0/EXT1 wake, check RTC-capable GPIO13. Expose boot-held, raw/stable, held and last-hold duration, edge events and release readiness in serial and existing status JSON. No actual wake source armed, no sleep entry, gestures or FIELD activation yet; BENCH remains awake for OTA. Isolated exact debounce implementation tested with native C++11; full Arduino build and actual button/hold recovery pending. No protected radio, OTA or artwork changes. Next: owner physical button tests, qualify BL current/drive and panel sleep, then controlled timer/GPIO sleep lifecycle with safe OTA recovery before enabling bench-sleep capability. Clock stage follows.


**GUI V1.00 source import (owner requested 2026-11-26):** [gui-port.md](gui-port.md), source commit [3b15117](https://github.com/bigbadevilaussie-hue/Moto-Mate/commit/3b15117c0b6705d74ec3dc77b55445b0f7baa501). Pinned donor GUI V4.54 dashboard/layout copied and renamed Moto-Mate, read-only OSA commissioning status, no fake Dyna PASS, no live legacy control paths. Legacy report/ride window source retained for adapter work; actions gated and explicitly pending. Firmware remains V1.24. Host/GUI stage remains open: iMac launch/visual test, authenticated maintenance/OTA/serial/settings, actual log/telemetry/analysis integration and regression tests outstanding. Launch from repo root with `python3 motomate.py`; existing CLI OTA unchanged. No OSA or donor modification/deployment.


**V1.25 — Stage 10 codec and identity foundation (2026-11-26):** REBUILD donor serialization/identity, not donor CSV or duplicated gzip. Added explicit little-endian schema-1 header and all 12 typed record encoders, CRC/structure checks, header/record/filename consistency, identity blob validation and checked 32-ID reservation API (commit then reopen/readback; single storage-owner caller only). Maintenance-only read-only NVS identity status; absent identity is expected on existing bench, no automatic provisioning/erase, no call to reservation API or file writes. No RTC reservation continuation, trip checkpoint, provisioning, append/seal/recovery, field activation or OSA change yet. Native fixture/negative-case test source added; actual C++ execution and Arduino compile/OTA pending. Existing schema bytes unchanged; diagnostic field extensions require separate contract review. No splash, Wi-Fi/BLE, sleep or partition change. Next: verify codec test, implement explicit trusted provisioning and RTC reservation continuity, then checked spool/recovery with failure injection.


**V1.26 — identity commissioning (2026-11-26):** V1.25 owner native codec PASS, compile 919597 B / 47912 B, OTA image 920064 B app0→app1 boot 1f867448, NVS 86 entries and LittleFS 8192 B used unchanged, identity unprovisioned and writes disabled. V1.26 adds explicit authenticated BENCH /log/identity GET/POST and /log/test-sequence POST; existing OTA transfer code unchanged. Initial generation only on owner-confirmed no prior schema-1 OSA history, host CSPRNG and exclusive durable registry creation (tools/moto_log_identity.py --initial; default ~/Documents/MotoMate_log_identity.json, preserve and import into future OSA registry). No automatic reprovisioning; existing mmlog namespace, nonempty/failed FS or invalid identity blocks initial write. No replacement/identity-loss recovery workflow yet. Test sequence consumes real IDs without files, NVS high-water commit/reopen/readback in blocks of 32, CRC-protected RTC cursor retained only after qualified deep sleep, matched to NVS before allocation; reset skips remaining block, errors fail closed. No routine sleep path mount/NVS writes, no production file writes, no trip checkpoint/spool yet. Current manual-reset entry to 30/30 bench clears cursor; physical qualified deep-sleep retention test deferred until allocation is integrated with an eligible wake. Native cursor test source added; actual execution/compile/OTA pending. No splash, protected radio, partition or sleep cadence changes. Initial registry is not a live OSA integration; explicit owner assertion required because schema-1 OSA ingest does not yet exist.


**V1.27 — explicit BENCH transactional spool (2026-11-26):** V1.26 owner compile 924405 B / 47936 B, native cursor PASS, OTA 924912 B app1→app0 boot c2325f09; initial identity persisted and host registry created; allocation 1,2 then manual reset allocation 33/high-water64 verified. Qualified RTC deep sleep retention pending. V1.27 adds single-owner POSIX LittleFS create exclusive, append with expected offset/header, checked write/fsync/close/readback, bounded contiguous-record scan and no-overwrite seal/recovery. Explicit authenticated BENCH write one real boot observation, leave .open or seal; explicit targeted recovery by sequence after reset. No fabricated sensor fields or UTC. Short/corrupt tail preserved whole as .rec, no safe records/header-only as .bad (conservative no deletion). Unknown files untouched. Native scan fixture source; real VFS/fsync/O_EXCL, compile/OTA, near-full, power loss and corruption tests pending. No automatic boot scan, production append scheduling, trip checkpoint, OSA transfer, ACK/delete, partition, display or radio changes. Test with tools/moto_log_spool.py --seal, --open then reset and --recover SEQUENCE (decimal or 0x hex). Test files consume real identity and space, remain until future verified host receipt. 16KiB reserve plus provisional 4KiB margin for commissioning only. No claim of durability from API alone.


**V1.28 — read-only BENCH transfer (2026-11-26):** V1.27 owner compile 931577 B / 47936 B, native spool fixture PASS, OTA image 932080 B app0→app1 boot a4f72fef, explicit seal seq65 and open/recover seq66 both 128 B/1 record complete. OSA isolated codec/receipt 9f35ce6 owner regression 9/9 PASS. V1.28 exposes authenticated idle BENCH GET /log/descriptor and /log/data with exact generation/sequence, sealed monitor .bin only, no active/ambiguous siblings. Checks header/record CRC, BENCH flag, factory MAC, size and SHA-256 in 512-byte chunks; streams raw using existing WebServer; no whole-file buffer, writes, ACK or deletion. Explicit LAN pull from isolated OSA CLI, not production transport; synchronous HTTP only in existing maintenance, no routine wake or radio changes. No restart/deployment of OSA server. Compile, OTA and actual device-to-host transfer pending. Physical torn-write, corrupted-tail, near-full, qualified RTC retention and full field lifecycle remain open. No splash or partition changes.


**V1.29 — exact receipt BENCH deletion (2026-11-26):** V1.28 owner compile 935081 B / 47936 B, OTA 935584 B app1→app0 boot 06e90df8, both seq65/66 delivered twice with identical hashes (65 3337764db9452c4176abff6e60e762198d862aa01c471986566e89a9d651e17f; 66 2a0666d272f005d9e86eaf7113b3ead6fda19f9aa5233f594a902c4d813fcae2), two originals/two receipts/two locks no staged files. V1.29 authenticated idle BENCH POST /log/ack takes generation, sequence, status=durable, exact compact descriptor JSON in firmware field order; recomputes sealed file CRC/MAC/identity/size/SHA-256 before unlink; rejects ambiguous monitor/ride siblings, refreshes space and verifies all known representations absent after removal. Only monitor .bin, no recovered/unknown deletion. Missing file returns 409 absent_unverified, never falsely proves prior deletion; no persistent tombstone or extra NVS wear. Host key is trusted commissioning authority on local LAN (not cryptographic proof of host fsync; plain HTTP not final field security). Host CLI explicitly revalidates/resyncs durable archive, negative tests before separately requested deletion. No automatic ACK, deletion or wake, no radio/partition/display change. Compile/OTA, negative and positive device tests, reset verification pending. Power-cut durability and FIELD lifecycle still unqualified.


**V1.30 — bounded automatic BENCH telemetry monitor:** V1.29 owner compile 937537 B /47936 B, OTA 938048 B app0→app1 boot fe8443f7, host 12/12, hardware negative ACK 9/9 (re-pull verified each), exact deletion seq65 PASS, repeat 409 absent_unverified PASS, control seq66 unchanged SHA PASS. Reset after removal not explicitly confirmed; physical torn-write/power-loss pending. V1.30 explicit volatile authenticated monitor session count1..4, first sample after 3s then nominal 300s without catch-up, one file, commit each record and seal at target/explicit stop. Fresh cached SHT temp/humidity and qualified MPU reference delta only; ADC battery unqualified and GPS absent remain null, UTC only qualified clock, actual boot/monotonic, BENCH maintenance state. No startup/ordinary sleep writes or new wakes; Wi-Fi loss, reboot/profile change, storage fault cancel and preserve open evidence for explicit recovery. Other log actions gated during session. No production logging, protected OTA/radio/display change, trip scheduler or OSA live deployment. 16KiB reserve plus provisional 4KiB margin, no automatic retry. Native cadence test and compile/OTA/actual records pending. Test tools/moto_log_monitor.py IP --start 1 then --status after first 3s, pull reported sequence using existing OSA CLI. Multi-sample real cadence separately.


**V1.31 — opt-in sleep logging qualification:** V1.30 owner compile 941453 B /48032 B, OTA 941952 B app1→app0 boot 8407bb36, native scheduler PASS, seq97 one sample all absent sensor/UTC null, seq98 two records 176 B SHA 83705306543f7f3c7c1aa5831d2462c1a622a1f3c11d69749d534dfcca146ad1, mono469467/769468 ms same boot interval300001 ms. V1.29 seq65 absent verified after repeat, seq66 unchanged; reset chronology not explicitly confirmed. V1.31 adds explicit authenticated sleep arm in existing settings word bit3 (only BENCH+sleep; old firmware rejects armed word, normal consumed word backward compatible). Manual reset consumes both flags in same existing settings write. Three consecutive 30/30 cycles each attempt one BENCH sample at 3s..10s, mounted only on opt-in cycles, seal before sleep, serial sequence/high-water/timing evidence. Separate retained bench magic allows qualified timer continuation only; stop logging after three or any log failure, ordinary radio-free 30/30 continues until button cancels. No routine sleep mount, no extra radio wake, no retries or log-dependent sleep cancellation, no field enable. One file per wake is deliberate qualification only, not production rotation. Existing maintenance activity code retained; no qualified battery/GPS. Native and physical compile/OTA, actual RTC cursor retention, 30/30 timing and current tests pending. Storage synchronous operations not hard real-time bounded; existing awake deadline remains. Test tools/moto_log_monitor.py IP --sleep-arm then manual RESET; record three serial results, exit via GPIO13 only after third result and sleep. Expected contiguous IDs in one NVS reservation; pull exact sequences after maintenance. Do not use ordinary power comparison with opt-in logs enabled.


**V1.32 — S3 reduced-TX qualification, single-variable radio change:** V1.31 owner compile 942929 B /48048 B, OTA 943440 B app0→app1 boot f37693a9, native sleep policy PASS; explicit arm persisted. Three successful pre-sleep files seq129/130/131, high-water160 unchanged, elapsed375981/269808/376356 us; first two pre-sleep times30007172/30006856 us. All three 128-byte originals transferred with durable receipts after reset (SHA-256 respectively 0b8c220eb37f5e589e829c0d2b3ca8e2a54af4faeea5e831e463dfa4dd2fdcf2, a351609c51bfde118a8af83ec1252144589a6e093dfc298deb43fe8206ea2664, aec4c239a3544bcb19b12248346c58f0879e8e414e2efdb58b60a3163dd15c49). Storage/RTC retention PASS; cycle4 no-write observation not supplied. GPIO13 returned to maintenance but repeated Wi-Fi attempts failed WL_DISCONNECTED=6; reset recovered first attempt1060 ms RSSI -46 dBm. Root cause unproven. Owner directed continued work and reduced TX investigation. ESP-IDF 4.4.7 ESP32-S3 API accepts quarter-dBm power units (34 = requested 8.5 dBm; actual calibrated ceiling may differ); donor Bike-Mate V6.49 uses 34. Unexpected Maker documents full-power instability on certain S3 boards (https://help.unexpectedmaker.com/docs/boards/wifi-stability-issues/); not proof for this generic SuperMini. V1.32 changes only requested TX ceiling 80→34, firmware version and documentation; retain IDF teardown, radio guard, retries, DHCP, BLE and sleep logic unchanged. No field readiness claim. Compare ordinary 30/30 sleep exit via GPIO13 and cold/reset maintenance, without enabling log writes; record actual TX readback, first attempt connection latency, RSSI and outcomes. One pass is not reliability proof; electrical energy unmeasured. Production logging and collection work can continue with radio recovery marked open pending repeated successful recovery.


**V1.33 — multi-wake monitor file qualification:** V1.32 owner verified 8.50 dBm readback, OTA boot connected1042 ms RSSI -42, ordinary 30/30 cycle1 pre_sleep30006458 us, cycle2 GPIO13 cancel then first attempt1073 ms RSSI -43. Initial recovery PASS; no proof of root cause/repeat reliability. TG0WDT_SYS_RST before armed sleep start observed, cause unconfirmed. Ordinary sleep no FS/log writes observed. V1.33 keeps 8.5 dBm and existing radio/display behavior. Opt-in three-cycle sleep test now appends one sample per wake into ONE .open file, seal after third. CRC-protected 88B RTC checkpoint contains exact encoded header+count; only qualified timer wake continuation can use it, consumes before mutation, checks current generation/high-water/MAC/version/count, exact on-disk header/size and full CRC-valid prefix, no sibling; cold/reset/button/failed write discards continuation, preserves .open for explicit recovery, no automatic resume interrupted writes. No extra NVS checkpoint. Nominal schema monitor period remains locked 300000ms; accelerated 30/30 BENCH test is not production cadence. Expected 224 B sealed three-record file, distinct boot IDs, unknown UTC means no inferred elapsed time between boots. Native test every-byte corruption; actual hardware test pending. No automatic recovery scan, production 24h rotation, power-cut proof, real energy savings or FIELD activation. User should record sequence from serial; after third sleep enter maintenance and pull same sequence. If interrupted before final seal, explicit test-recover using reported sequence; never delete original.


**V1.34 — maintenance recovery and candidate inventory:** V1.33 owner reported OTA image944896 B, 8.5 dBm boot1040 ms RSSI -43; cross-wake seq161 high192, second wake163268 us, third231193 us, sealed224 B SHA 11513be87729199a0ac571b46d45543c988ed25fa868db3f771fbdd2c0662f43, initial pull timeout then durable PASS; records ordinal0/1/2 boot1a800400/d25c7eaf/c10b9e44 mono3191/3143/3165 ms, validity0. Cross-wake aggregation qualified; native checkpoint result, exact exit/reset chronology and cycle4 no-write not supplied. V1.34 once per boot, after successful Wi-Fi existing BENCH maintenance entry, scans at most 256 directory entries and recovers at most16 canonical current-generation .open monitor files, after closing directory. Require factory MAC, valid BENCH header, reserved identity, bounded size, no siblings; existing CRC scan classifies complete→.bin, damaged tail→.rec, header-only/bad→.bad; invalid headers/ambiguous/foreign files untouched. Incomplete directory enumeration refuses mutation. Original bytes retained, no truncate/delete. Authenticated GET /log/inventory returns bounded candidate sequence strings and counts for open/recovered/bad/other, complete flag. Candidate names are NOT validated receipts: existing descriptor/data checks still mandatory, host refuses incomplete inventories and pins identity per pull. OSA CLI --inventory / --all (retains device originals, no live OSA integration or automatic wake), malformed candidate rejected, per-file failure reported. .rec/.bad not yet transferred by firmware; no production collection, field logging, power-cut proof or automatic ACK. No radio changes. Native/parser/host/hardware tests pending. Create explicit open boot file, reset, verify auto recover and --all without manual sequence entry.
