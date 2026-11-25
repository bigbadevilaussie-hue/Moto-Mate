# Moto-Mate — AI Handover

Last updated: 2026-11-25  
Status: pre-port baseline; documentation only.  
Read this file first when starting a new Moto-Mate chat, then inspect current Git HEAD and the relevant source files. This is the owner's latest direction, not a substitute for live repository contents.

## 0. Project Prime Directive — SAVE THE BATTERY AT ALL COSTS

The owner's overriding requirement: "Save the Battery at all costs. dont let the tool be the bigger drain. collect all data to diagnose the battery and chanrging system."

Moto-Mate is a battery and charging-system diagnostic instrument. It must not become a significant parasitic load or contribute to the problem it is diagnosing. Battery preservation takes priority over convenience features, display uptime, connectivity and unnecessary activity.

Engineering implications (implementation subject to owner direction and evidence):
- Minimise total current at the AGM battery terminals, including regulators, peripherals, leakage, LEDs, GPS, display, and radio activity; the existing parked deep-sleep target is <=1 mA, not yet verified on Moto-Mate.
- Account for wake-up energy and radio/upload retries as well as steady sleep current; measure average parked consumption over representative wake cycles. Prevent indefinite awake states or retry loops when parked.
- Capture diagnostically useful battery voltage, timestamps, temperature, rest/discharge trends, cranking voltage sag, charging voltage and ride/engine-state transitions where the hardware and sampling permit. Preserve useful raw readings and existing diagnostic history. Do not claim to measure current, capacity or state of health directly without suitable hardware and evidence.
- Preserve logs across sleeps and failed uploads; defer nonessential activity when battery preservation requires it. Retain the proven donor software wherever possible and do not modify protected BLE/Wi-Fi code without demonstrated necessity and discussion.
- Validate the effect of changes on the battery budget before accepting them. Exact low-voltage cutoffs, sampling cadence changes and retry limits require review against the donor and owner agreement; do not invent or implement them in this handover.

## 1. Major project rule: DIRECT PORT, NOT A REWRITE

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
| Destination | [Moto-Mate](https://github.com/bigbadevilaussie-hue/Moto-Mate) | main, e6828935eb18d9df960fab3260684791eba1315a, 2026-11-19, firmware V1.00 |
| Software donor | [Bike-Mate](https://github.com/bigbadevilaussie-hue/Bike-Mate) | main, bc5766c319da25622db1f2c86b58339afd07d5ad, 2026-11-03, firmware V6.49 |
| Integration server | [OSA-Server](https://github.com/bigbadevilaussie-hue/OSA-Server) | main, 434b65d6bf74734f8b4b13224472c53ad78f042e, 2026-10-30, V1.33 |

Bike-Mate V6.49 includes the two-second engine-stop debounce. Recent donor history also includes RTC settimeofday, simplified sleep interval/settings, and TinyGPSPlus. Read the current donor implementation before proposing changes, rather than reconstructing behaviour from old documentation.

## 4. Non-negotiable protected code and toolchain

**BleManager.cpp / BleManager.h are protected.** The owner spent weeks getting BLE working. Do not refactor, optimise, rewrite, switch libraries, or alter timing, callbacks, UUIDs, packet framing, handshake, ACK handling, retry behaviour or file transfer semantics for cosmetic reasons. If a change is demonstrably required by the S3 or agreed functionality, identify evidence and discuss it with the owner before touching protected code. Preserve OSA and GUI compatibility end to end.

**Arduino ESP32 core 2.0.17 ONLY.** The owner explicitly locked this because of macOS toolchain and NimBLE/BLE compatibility problems. Do not upgrade to 3.x or migrate BLE stacks; the Moto-Mate sketch currently includes the Arduino BLEDevice/BLEServer/BLEUtils/BLE2902 headers. The reference to NimBLE describes past compatibility trouble, not a request to switch libraries.

**Wi-Fi/BLE coexistence is protected.** Never run both at once. Stop BLE, wait 500 ms, then start Wi-Fi as documented. The Bike-Mate WifiManager.cpp IDF teardown/reinitialisation sequence is documented as proven 72/72 on Opal. Preserve it and existing retry behaviour. Keep documented 8.5 dBm TX power baseline; validate on the new board rather than guessing.

## 5. Current development state (owner-confirmed)

- A test compile with all required peripheral/library includes has already passed under core 2.0.17. This is library/toolchain compatibility evidence, not proof of runtime or electrical operation.
- No firmware port or GUI port has been started. No other development beyond the existing baseline has been reported by the owner.
- Moto-Mate motomate.ino is a minimal include-only skeleton with empty setup() and loop() at the reviewed revision. Config.h defines the GPIO map and V1.00. Buzzer.cpp/h contain an existing active-buzzer baseline.
- Veroboard hardware design, wiring guide, custom part JSONs and V4 OpenSCAD enclosure exist. Physical assembly/test status has not been confirmed.
- The AI explored repositories read-only before this documentation task. No code copied, changed, compiled, flashed or tested by the AI.
- Bike-Mate GUI screenshot supplied in chat shows GUI v4.52, disconnected / NO SIGNAL. It is the visual reference, not a hardware test result. No screenshot binary is stored by this handover.

## 6. New hardware: use Moto-Mate files, not donor pin definitions

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
- No physical current, wiring or compile verification was performed by the AI.

## 7. Software donor map

Bike-Mate's bike_mate.ino owns the state machine, RTC, wake/sleep and setup/loop. Its modular firmware includes BleManager, DisplayManager, Sensors, GpsModule, Buzzer, RideLogger, RideStorage, WakeLogger, Settings, WifiManager, WebServer, OtaManager, DriveUpload, WifiMail and SerialBuffer. The C3-to-S3, OLED-to-TFT, NTC-to-SHT40, MPU and GPIO/power adaptations are the areas to inspect, not a license for sweeping changes. Keep the original protocol and file formats unless explicitly agreed otherwise.

The GUI is a Python/Tk application launched by bikemate.py with the package bikemate/ (app.py, ble.py, config.py, drive.py, ota.py, reports.py, ride_picker.py, ride_finder.py, dynatune_window.py, serial_window.py, state.py, weather.py, widgets.py and helpers.py). Preserve the existing GUI layout and functionality: ACC/engine/warn status, connection status, large voltage reading, temperature/storage/time, voltage and temperature graphs, settings, reports, maintenance, OTA. Branding and new telemetry changes only as requested. GUI helper modules were located but not reviewed in depth.

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

1. Read this handover from Moto-Mate main.
2. Confirm current Moto-Mate, Bike-Mate and OSA revisions and inspect relevant source files.
3. Follow the direct-port rule, protected BLE/Wi-Fi code and core 2.0.17 lock.
4. Take the technical lead on requested patches/porting. Ask only for missing material hardware or test facts, and do not make speculative changes.
5. Report actual results and update this document when project decisions change.

Current next action: owner selects the first implementation task from TODO.md. Static audit complete; no firmware/GUI/OSA code port or patch has been performed.
