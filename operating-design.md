# Moto-Mate — agreed operating design and development direction

## Owner rule: Moto-Mate governs OSA

**OWNER RULE — 2026-11-26: Moto-Mate dictates how OSA works from now on.** The owner explicitly authorises the technical lead to fix and rework OSA to meet Moto-Mate's requirements, rather than constrain the new firmware to OSA's historical architecture. This supersedes the earlier requirement for separate scope approval for OSA engineering changes. It does not authorise arbitrary changes to Bike-Mate firmware, protected ESP32 radio sequences, or other device firmware. Preserve the battery-first prime directive, the ESP32's autonomy, and OSA's role as sole BLE broker. Preserve working services and user-facing behaviour for Bike-Mate, Fan-Mate and Audio-Mate, especially Alexa telling the owner to check the bike, existing alerts, storage and OTA workflows. Compatibility is a behavioural requirement, not a mandate to retain flawed internals. Do not silently remove features or change alert routing. No automatic deployment: use small testable changes, regression tests and an owner-controlled cutover with rollback.

Proposed OSA V2 direction: modular core and explicit HTTP router, one coordinated BLE central, device-specific adapters, independently testable clock readback/drift, durable commands and transfers, storage integrity, GitHub synchronisation and alert delivery. KEEP / ADAPT / REBUILD / RETIRE based on evidence; no big-bang cutover. Develop isolated from production on a separate branch/test port (proposed 9998), with external side effects disabled in tests. Keep port 9999 and legacy contracts at production cutover where needed. Do not count a GATT write as application acceptance or a cached read as a new sample. Avoid waking or keeping Moto-Mate awake for GUI convenience. The architectural details remain subject to implementation/testing, not yet proven.

Verified owner-reported baselines (2026-11-26): Moto-Mate firmware V1.06, source 6dfade2b63bbfbc112ffc07cb5ddb1f4b6854f60, compiled 981341 B flash / 46828 B static RAM, uploaded with esptool 5.6.0; repeated USB commissioning passed sprite/splash, dual OTA/LittleFS, Wi-Fi, Opal clock and Wi-Fi OFF, zero service overruns, reached BENCH_IDLE at 90 seconds. BLE/OSA handshake, physical peripherals, deep sleep and current remain untested. OSA V1.39, ca8f91eb215a1bdd2d947c181306ab7bf5f39980, deployed on iMac: /moto/status, /moto/time and /bike/status all responded successfully. Moto BLE explicitly not implemented. Existing Bike-Mate clock diagnostics preserved. Full other-device regressions not yet demonstrated.


2026-11-26. Living decision record for the owner and future AI sessions. Read with [aihandover.md](aihandover.md), [README.md](README.md), [port-plan.md](port-plan.md) and [TODO.md](TODO.md). This document supersedes conflicting older architectural suggestions. It does not authorise unrelated patches, lift protected-code rules or certify hardware.

Status vocabulary: **AGREED** = explicit owner instruction or approval; **DESIGN** = proposed engineering implementation of that requirement, subject to verification and any outstanding approval; **OPEN** = decision still required; **BASELINE** = observed source or owner-reported test, not a new requirement.

## 1. Mission — prevent and predict issues

**AGREED:** SAVE THE BATTERY AT ALL COSTS. Moto-Mate is a predictive battery and charging-system diagnostic instrument. Do not let the diagnostic tool become the bigger battery drain. Collect useful evidence, detect emerging deterioration, alert the human and help prevent breakdowns, rather than merely recording them after the event.

Match or beat Bike-Mate's owner-reported 1 mA parked input current; measure complete wake-cycle energy as well as deep-sleep current. Do not confuse an ESP32-only measurement with the whole system. Do not promise a reliable starting reserve from voltage alone.

## 2. Code architecture and port method

**AGREED:** Build a fresh setup/loop framework first, then introduce each real subsystem, using donor code that works well and replacing poor implementations. Owner explicitly approved the framework and V1.04 was committed. Do not transplant Bike-Mate's 1,262-line sketch. No firmware reuse percentage target; clean code and working behaviour matter. Existing functionality is largely working and should be made better, not gratuitously reinvented.

Classify each candidate KEEP / ADAPT / REBUILD / RETIRE. Inspect current source, real callers, dependencies and historical fixes before deciding. Integrate in small compilable/testable increments; no fake-success stubs, blind imports, duplicated state/timer ownership, dead code, arbitrary waits or uncontrolled retry loops. Audit all important setup/loop/service/callback paths. Preserve necessary proven waits.

Core Arduino ESP32 2.0.17 ONLY, LovyanGFX 1.2.36. Protected BleManager and proven Wi-Fi recovery sequence remain protected; demonstrate need and get separate approval before altering. No simultaneous BLE/Wi-Fi; preserve donor handover waits (at least 500 ms after BLE stop, existing donor delay 800 ms) and 8.5 dBm Wi-Fi baseline. Preserve live protocol and data contracts unless jointly agreed. GUI visual layout largely a direct copy; improve actual workflows and post-processing.

## 3. Three distinct responsibilities

**AGREED:** GUI is a dumb terminal for live device operations, but the post-processing/report machine.

| Layer | Responsibility |
| --- | --- |
| ESP32 | Wake, read button and sensors, make safety and operational decisions, control hardware, timestamp and capture evidence, preserve data, send bounded transfers, run maintenance/OTA, sleep. |
| OSA | Own sole BLE link, cache last device data, broker/queue commands, sync time, safely receive/acknowledge/archive files, GitHub delivery, notify human. |
| GUI | Render reported state, operate maintenance and OTA, settings, serial terminal; historical graphs, diagnostic analysis, trends and predictive reports on the Mac. |

Normal GUI requests are cache reads, not a reason to wake the ESP32 or keep it awake. OSA does not dictate the bike's power policy. ESP32 must still do essential real-time safety, engine-state and logging work; defer expensive statistical analysis to the Mac. During maintenance retain existing working direct Wi-Fi HTTP endpoints where practical. Do not add a second BLE client.

## 4. Wake-first decision framework

**AGREED direction:** wake up, check button, check voltage, decide what to do (ride, routine monitoring, maintenance, PANIC or FAULT), then return to business/sleep. **DESIGN:** one cooperative coordinator and explicit state transitions, not a separate blocking loop for every mode.

1. On cold boot, timer or button wake, capture cause early and establish safe GPIO/output state. Start monotonic wake budget immediately. No unconditional slow splash or radio bring-up on a parked timer wake.
2. Observe button early, debounce and collect gesture events without blocking sensor acquisition or safety checks. A pending double-click window must not block the main loop. Wake by active-low GPIO13; first/second press can arrive around startup, test actual latency.
3. Obtain valid battery voltage promptly. Read other sensors only as required with appropriate cadence. Never substitute a fabricated reading for missing/invalid data.
4. Evaluate protection, then engine evidence, then pending requests and required work. Distinguish genuine critically low parked voltage from crank sag and ADC failure. Reassess continuously while awake.
5. Run the needed services with explicit deadlines/transaction boundaries. No catch-up bursts or repeated full peripheral initialisation without reason. Preserve useful ride/transition evidence.
6. Central power decision: sleep only after required work and safe transaction closure; disable unused rails, backlight, radios and GPS, configure wake sources and holds. If no work, return promptly to deep sleep. Handle held button without an endless wake loop.
7. Measure maximum loop gap, service execution duration, overruns, button latency, sensor cadence, wake duration, free heap and energy. A cooperative deadline cannot preempt a synchronous blocked call; bounded APIs and safe failure handling are still needed.

**DESIGN:** keep operating profile, activity, protection and power status distinct; avoid one enum combining every possible combination and avoid global flag soup. Coordinator owns transition and scheduling policy; PowerManager owns physical sleep eligibility/shutdown. Services report completion/failure and temporary sleep inhibitors, not independent deep-sleep calls. Add real states and handlers as subsystems arrive. Preserve active ride logging and safe OTA/flash/file transactions; no forced sleep in an unsafe transaction.

| Dimension | Intended values |
| --- | --- |
| Profile | BENCH, FIELD |
| Activity | Assessing, monitoring, rider ready, battery check, riding/arming/cooldown, BLE file transfer, maintenance, OTA |
| Protection | Normal, warning, PANIC, FAULT; independent of activity as appropriate |
| Power | Awake, sleep pending, deep sleep (actual CPU asleep), bench idle (test only) |

These are design concepts, not the exact final enum/API or a claim that V1.04 implements them.

## 5. BENCH, FIELD, sleep and fault policy

**AGREED owner rule (2026-11-26): fixed parked cadence, 300 seconds actual deep sleep and at most 10 seconds awake, returning sooner when work is complete. No peak/off-peak, time-of-day, timezone or DST-dependent wake intervals. UTC timestamps are evidence; elapsed-time scheduling is independent of civil time. Retire the legacy 04:00 wall-clock collection trigger; retain elevated 24-hour collection as elapsed-time due work, retry pending delivery on eligible wakes, mark completion only after verified acknowledgement. Ride, explicit maintenance, protection and safe transaction completion remain distinct exceptions. No adjustable parked cadence in NVS.**

**AGREED owner rule (2026-11-26): BENCH/FIELD is an NVS setting, never an OTA build variant.** Profile, bench sleep and verbose flags persist across application updates; effective activity and safety state remain runtime-owned. V1.12 implements the NVS contract; FIELD and bench sleep remain capability-gated pending runtime integration. See [settings-design.md](settings-design.md).

**DESIGN:** BENCH and FIELD are profiles of the same production firmware. BENCH is not synonymous with maintenance or always awake. Independent bench sleep enable and verbose diagnostics allow real sleep, wake, RTC and current tests without abandoning the BENCH profile. FIELD enforces battery-preservation rules; cannot accidentally retain bench stay-awake override. BENCH on USB may explicitly remain awake for development. Do not assume USB bench can prove 12 V input current or crank brownout immunity.

**AGREED owner direction (2026-11-26):** routine parked operation is 300 seconds in deep sleep, wake, perform necessary work and return to deep sleep in under 10 seconds; faster is better. The 10-second target covers boot, sensor acquisition, assessment, logging, due communications and shutdown, not merely the application loop. Ride, maintenance, PANIC/FAULT and safe transaction completion require distinct budgets; do not force sleep mid-flash or unsafe storage commit. Defer unfinished noncritical work persistently at safe boundaries rather than extend ordinary parked wakes. Five minutes means sleep duration, not start-to-start period. Exact subphase allocations and exception deadlines remain design proposals.

**AGREED:** donor BLE/Wi-Fi choices and scheduled 04:00 file collection were motivated by power consumption, not immutable requirements. Transport, cadence, batching, clock sync and fallback may change based on measured total energy per successful operation, reliability and diagnostic needs. Compare BLE and Wi-Fi on the actual S3 including boot, discovery/association, transfer, ACK, retries and shutdown; no assumption that lower instantaneous current means lower total energy. No radio changes or protected donor implementation changes are made by this decision record.

**DESIGN — instrumentation:** record monotonic microseconds per boot/wake phase, total awake duration at sleep entry, wake cause, attempted/completed/deferred work, radio/transfer timings and deadline misses. Persist compact counters and last completed wake summary; report to OSA on next successful contact without extra wake/radio solely for statistics. Track minimum, median, p95, maximum and fraction under target on host from actual samples; distinguish device monotonic time from host UTC and measure full boot-to-sleep and whole-board energy independently at supply input. OSA displays last observed state and expected next wake, not fabricated live sleep state. Start with no-work baseline, then sensors, BLE/Wi-Fi, backlog and unavailable OSA cases. Bench USB settling and verbose serial diagnostics are not representative field timing.

Bench cadence, override limits, field deployment default and precise exceptions remain OPEN. V1.04's test-only settings and V1.06's 90-second BENCH commissioning do not represent field sleep performance.

**AGREED:** PANIC alerts the human. **DESIGN:** confirm valid critical battery condition; record voltage/time/temp and diagnostic context; cut unnecessary peripheral loads; send priority alert via OSA if available, record acknowledgement, retain undelivered alert for bounded future retries, sleep as soon as safe. Reuse OSA notification infrastructure (iMessage, reminder, spoken announcement, Alexa). No endless radio attempts or buzzer/display activity. Distinguish device acceptance, notification dispatch and human receipt. Recovery with hysteresis and confirmed readings; avoid duplicate alert storms. FAULT must recover safely with limited retries, retain evidence and alert as appropriate. An invalid ADC sample is not a flat battery. Exact threshold, severity, retry/escalation and alert transport/fallback remain OPEN. Do not interrupt unsafe OTA writes.

## 6. Work priorities

**AGREED:** GUI/OSA requests for ordinary data are low priority; OTA high priority; 24-hour file suck is scheduled priority work, not just routine GUI telemetry. Battery protection and alerting override convenience work.

| Class | Work |
| --- | --- |
| Critical | PANIC, safety/fault handling and transaction safety |
| High | OTA; preserve essential engine detection and ride capture |
| Scheduled / elevated | Elapsed 24-hour file collection, pending delivery recovery |
| Interactive | Local button requests, maintenance operations, settings, manual collection |
| Low | Ordinary GUI/OSA live data polling and routine status requests |

**OPEN:** exact relative priority of daily transfer vs OTA/ride/button/settings, absolute and inactivity deadlines, retry limits. Earlier assistant proposed daily P2 below OTA P1; owner did not expressly approve numeric assignment. Do not treat that ranking as settled. Active user maintenance should allow its requested operations, not starve them behind routine transfers. Prioritise at safe boundaries; low-priority polling must not renew wake budget. A queued request is not successful execution. Daily upload remains due until confirmed receipt, not merely attempted.

## 7. Owner's maintenance workflow — one session

**AGREED:** owner sits at iMac, compiles new firmware, requests maintenance remotely OR presses the physical maintenance button/gesture. Then within the session suck logs, send OTA, open serial page and do basic checks/settings; log out and bike goes back to business. Bike is close to iMac in carport; local button is a practical alternative to waiting for the scheduled wake. Most of this already works: make it smoother and more reliable, not a new GUI or protocol for its own sake.

**DESIGN:**
- Remote request is queued on OSA until a normal device wake; BLE cannot wake a deeply sleeping ESP32 remotely. Physical button wakes locally. Report pending, accepted, ready, executing, completed/failed and expiry/cancellation accurately.
- BLE-to-Wi-Fi transition follows proven protected sequence. Explicit maintenance session stays available for collection, OTA, console and settings. Serial page opens when ready and reconnects after OTA. Preserve existing working HTTP settings/manual upload/serial paths; optional BLE settings outside maintenance is an additional future route, not a replacement for this session.
- Validate correct S3 application binary before asking bike to spend time awake. Archive/backup without delaying OTA unnecessarily once awake. High-priority OTA; explicit begin/write/end/abort, correct image/size, no false success from HTTP 200 alone; verify actual boot version and restore maintenance after successful reboot for checks. Do not sleep mid-flash or blindly reboot on failed upload.
- Clean logout/Exit Maintenance ends session, cancels or safely finishes work, clears overrides/requests, shuts down Wi-Fi, restores appropriate normal radio/activity and sleep policy. No needless reboot just to exit. Abandoned session has bounded inactivity and absolute time limits; routine GUI polling must not keep it alive forever. Avoid premature timeout while genuine user work is in progress. Handle ride start and PANIC safely.
- Existing endpoint names and UI layout preserved where practical. No approved new console layout, extra protocol or unconditional session auto-entry after every wake. Earlier proposal to remove all maintenance activation and do every task separately is superseded by the owner's session workflow.

**OPEN:** exact maintenance gesture (long press proposed, not approved), session durations, inactivity definition, cancellation/expiry contract, legacy URL OTA and fallback alert policy. Double-click battery check was expressly requested; single-click rider-ready is proposed to preserve the existing intent. Gesture debounce/hold/window timings need real hardware tests.

## 8. Data collection, delivery and clock

**AGREED priorities:** efficient BLE logging, rock-solid OTA, responsive wake button, fix severe time drift. Preserve evidence rather than optimise away diagnostic data.

**DESIGN:** separate monotonic deadlines (per-boot) from UTC/wall-clock sample timestamps; remove dead manual counters, validate clock sources/quality, don't infer elapsed unpowered time from saved epoch. Measure internal RTC drift across sleep against OSA. PCF8563 module is a candidate, not selected; avoid underside crystal soldering. No permanent RTC hardware selection yet.

Preserve wake/ride CSV and summary/wire contracts; correct known log state and GPS validity errors. Capture true crank sag where possible, not merely high-voltage engine-start detection. Sample voltage quickly enough for intended diagnostic evidence; slower temp/motion when appropriate. Record validity/quality, accurate sample times, gaps, state transitions. No fake data, no silent discard of real low voltage. Do not infer reliable engine-on from voltage alone during charging failure.

LittleFS mount must not auto-format on failure; recover sealed logs; checked writes and transactional compression, temp-to-final rename, preserve raw/sole copies until verified. One common safe gzip path. Bounded transfer batches, ACK only after verified receipt, safe retry and no silent loss, no duplicate/ambiguous filename deletion. OSA should own archive and GitHub push. Test missing OSA, interruptions, full disk, reset during compression, lost ACK, collision and backlog. All existing 47 audit tickets remain open until verified.

## 9. GUI and OSA audit — findings for porting

**BASELINE:** Bike-Mate firmware V6.49, GUI source V4.54, commit bc5766c319da25622db1f2c86b58339afd07d5ad; OSA V1.33 commit 434b65d6bf74734f8b4b13224472c53ad78f042e. Reviewed all 14 first-party package modules, launcher and packaging script; inspected relevant OSA BLE/cache/HTTP/transfer/notification paths. Static review only; no runtime GUI test or comprehensive audit of unrelated OSA services. Source links below are pinned and findings must be rechecked at implementation.

| GUI area | Candidate disposition / evidence |
| --- | --- |
| app.py / main dashboard | KEEP visual layout. ADAPT profile/activity/protection/power and reading freshness. Current 60 s stale logic can show NO SIGNAL during ordinary sleep, while ACC/engine/warn still show cached status. Hard-coded voltage colouring does not follow settings. |
| ble.py | CLEAN/ADAPT. Polls OSA cache every second; this is Mac-side, not BLE polling. Repeated cache reads append duplicate values and reset GUI seen time. Use actual sample identity/time and OSA age; distinguish asleep, offline, stale and actual new samples. Retired BLE client/loop are None. |
| app.py settings | ADAPT. Working HTTP settings in maintenance; retired direct GUI BLE path cannot work with None client. Retain dialog and validated fields; optional OSA-mediated BLE settings later, not required for initial maintenance flow. |
| app.py maintenance | ADAPT/REBUILD coordinator. Several polling paths and inconsistent timeouts; some network requests block Tk event loop. Day/night wake estimates obsolete. Request acceptance is not confirmed ready; no stale pending commands. |
| app.py / ota.py | ADAPT/REBUILD workflow. Direct HTTP POST currently active; ota.py is effectively an empty legacy module. C3 paths, stale binary warning can be bypassed, backup before upload, no actual reboot-version verification. Keep known working upload endpoint. |
| serial_window.py | KEEP console layout, reconnect across OTA; 2 s direct HTTP polling only in maintenance. Avoid overlapping refresh requests and stop on close/exit. |
| reports.py | FIX. Date map uses YYYY-MM-DD keys but day/2h lookup uses wakes_ prefix; fallback can show wrong date. Only newest file per day loses sealed segments. Week selects latest seven available days, not necessarily calendar week. Below-5V readings dropped; validate with quality rather than fixed rejection. Integer temperature conversion and row schema handling need compatibility checks. |
| ride_picker.py | COMPLETE. Last-24h filter uses calendar dates rather than rolling 24h; graphs placeholder, below-5V discarded, absent GPS speeds shown as zero. Preserve data validity and correct units/timing. |
| ride_finder.py | KEEP/COMPLETE. Lists archived gzip rides and shows first 50 lines of raw CSV; not full analysis/map. Avoid UI blocking; don't fetch directly from bike. |
| dynatune_window.py / app.py | REBUILD analysis, KEEP diagnostic visual board. DynaRide/DynaBike use fake PASS metrics; empty/all-IDLE checks can be reported Battery System Healthy. Never expose fabricated health assessment. |
| widgets.py | KEEP visual graph component; ADAPT time-aware samples, gaps, validity, actual axes and dirty redraw. |
| drive.py | ADAPT GitHub/archive/paths, retire confirmed unused legacy paths only. OSA owns log receipt; firmware backup should not delay OTA. |
| config.py / helpers.py / state.py | CLEAN donor C3 paths, branding, retired day/night logic, direct BLE imports/stubs, stale flags, global thread-safety and config validation. Keep credentials out of repo; review unverified TLS context. |
| weather.py / launcher / packaging | KEEP Mac-side functionality; adapt names/config as needed. |

Pinned [GUI source](https://github.com/bigbadevilaussie-hue/Bike-Mate/tree/bc5766c319da25622db1f2c86b58339afd07d5ad/bikemate); [OSA source](https://github.com/bigbadevilaussie-hue/OSA-Server/blob/434b65d6bf74734f8b4b13224472c53ad78f042e/osa_server.py).

OSA candidates (source line numbers at above pin, not authorisation to patch):
- /bike/status returns cache (926–938), alive <60 s; GUI polls it, while actual OSA BLE read every 2 s (881–897). Treat low-priority ordinary reads as optional, no wake extension. Preserve distinction between cache retrieval, device observation and sample timestamp.
- On connect OSA syncs time then subscribes before commands (798–823); preserve this proven ordering. Missing-file request precedes maintenance (825–849); coordinate priority and readiness. Existing pending maint/upload booleans (162–172, 1866–1878) clear on successful write rather than execution result and have no expiry; add bounded command lifecycle if approved.
- BLE receiver (588–726) validates length, not content CRC (donor CRC zero); equal-name/equal-size treated as duplicate without content equality, ACK queued twice after save (704–717), direct final write, duplicate branch resets different state object. Verify actual receipt/integrity, atomic file handling and single ACK. Manifest path (758–771) ACKs based on filename existence; not proof same content. ACK dequeue/write failure handling needs review. Preserve packet formats until coordinated changes approved.
- Existing human notification HTTP routes (1920–1952) dispatch reminders/iMessage/voice/Alexa; BLE telemetry callback (729–775) caches data, no demonstrated direct PANIC-to-notification mapping. Add explicit agreed alert path, bounded delivery and deduplication.
- Identity/replacement vs coexistence, IP, OSA routes and storage destination remain OPEN; no Fan-Mate disruption. No actual OSA/GUI code changed.

## 10. Predictive reporting — Mac side

**AGREED goal:** prevent and predict issues. **DESIGN:** use real archived data and learn this bike's own healthy baseline, compare like-for-like conditions over successive rides/days/weeks, detect persistent deterioration and give evidence-based recommendations.

| Candidate diagnostic | Evidence to analyse |
| --- | --- |
| Battery deterioration | Rested voltage trend, increasing true crank sag, slower recovery at comparable temperature |
| Possible parasitic load | Increasing rested voltage decline after surface charge settles; confirm with current measurement |
| Charging system deterioration | Ride voltage level, time below/above expected charge range, variability and historical trend |
| Starter/connection concerns | Repeated abnormal crank waveform or intermittent voltage drops; confirm with electrical tests |
| Poor recovery between rides | Declining rested pre-ride voltage, insufficient recovery, ride history |
| Data/device health | Clock drift, missing data, invalid samples, uploads, GPS validity, storage faults, firmware history |

Suggested display: HEALTHY / WATCH / WARNING / PANIC, with UNKNOWN/INSUFFICIENT DATA for missing or invalid evidence. GUI analysis may warn of trends; immediate device PANIC remains firmware protection policy. Show actual evidence, sample count, time span, quality, assumptions and uncertainty. Never imply component diagnosis, state of health, capacity, current draw or guaranteed future failure from voltage alone. Exact analytical algorithms, thresholds and training/baseline window remain OPEN. Do not invent a numerical failure prediction.

## 11. Host status contract (candidate fields, not approved wire schema)

Report profile, current activity, protection/fault, awake/sleep preparation and last known state, expected next wake (with uncertainty), sample timestamp/validity, firmware version, operation identity/status/result, pending log count and alert state. OSA cache retains last actual observation and age; asleep is inferred from last report and expected schedule, not live observation of sleeping CPU. GUI shows freshness and historical values honestly. Keep legacy fields and compatibility until agreed; no invented UUIDs/endpoints or new schema by this document alone.

## 12. Bench and hardware context

**AGREED:** Phase 1 breadboard ESP32/TFT on iMac USB-C, add sensors as needed, simulate motorcycle voltage with bench PSU and reviewed ADC divider; aim to test about 90% of code before locking circuit (goal, not measured coverage). Then owner builds stripboard and Phase 2 tests actual hardware. On-bike validation is later and requires approval.

**BASELINE:** owner reports ~6 V sag during 1050cc cranking, 12.9 V post-ride settling near 12.6 V for weeks; use as scenarios, not captured traces or universal thresholds. Mini360 measured 11 mA by owner, unsuitable as always-on candidate against 1 mA total target; test conditions not recorded. Primary regulator and voltage divider intended as only always-on 12 V loads; final rail design not locked. INA226 modules intended primarily for bench measurements, no decision to add to production. RTC candidate PCF8563 not selected. R11 existing 10k wake pull-up; test switch before adding capacitor. AliExpress first for parts. Reconcile diode, divider location, protection and 6 V dropout/hold-up against actual hardware. See hardware-inventory.md and schematic audit; no fabrication approval.

**Display:** seven splash modes (initialising, maintenance, BLE upload, Wi-Fi log upload, firmware update, rider ready, battery check), live white status/version and voltage/temp for double-click battery check. Static red/white/black artwork remains provisional; lock native 160x128 RGB565 asset after approval. No splash/backlight for routine parked timer wake. Riding/warning dashboards separate.

## 13. Implementation status and next work

V1.04 firmware commit 36bbb253622e65b85f1ad68cf309220756c684b9 contains new Runtime.h, setup/loop, isolated display diagnostics and native test source. Previous repository records native C++11 test pass; no new test performed in this documentation update. Owner Arduino compile and physical ESP32/TFT runtime not yet reported for V1.04. Last owner compile V1.03: 386841 B flash, 20528 B static RAM, 307152 B remaining. No field power test, GUI/OSA port or protected code patch. Bench-only V1.04 must not be deployed on bike.

Current checkpoint: **Current checkpoint — 2026-11-26: V1.16 owner BENCH runtime confirmed.** V1.13 compiled 817849 B flash / 46720 B static RAM; verified different-version OTA V1.10 app1 to V1.13 app0 and NVS initialisation. Physical TFT wiring corrected; V1.14 RGB/BGR correction accepted, logo visible. V1.15 typography owner-tested; V1.16 animated splash accepted, serial confirms two complete startups, temporary sprite released, NVS loaded, Wi-Fi connected in 1037/1071 ms and OTA ready, maintenance mode rendered. Reason for second boot not established. BL remains disconnected/unqualified; panel stays illuminated on bench. No measured heap low-water, full-system current, physical button wake, sensor or field validation. Current physical TFT is on the USB breadboard; reconnect momentary switch after rewiring (GPIO13 to GND).

Next: **V1.17 port-plan stages 2/3, first hardware slice:** ADAPT donor safe GPIO hold recovery and early button sampling to S3; retain single input owner, replace donor blocking release wait with monotonic 30 ms stable-release qualification. Prepare low outputs before releasing GPIO holds, restore RTC wake pin to digital mode on EXT0/EXT1 wake, check RTC-capable GPIO13. Expose boot-held, raw/stable, held and last-hold duration, edge events and release readiness in serial and existing status JSON. No actual wake source armed, no sleep entry, gestures or FIELD activation yet; BENCH remains awake for OTA. Isolated exact debounce implementation tested with native C++11; full Arduino build and actual button/hold recovery pending. No protected radio, OTA or artwork changes. Next: owner physical button tests, qualify BL current/drive and panel sleep, then controlled timer/GPIO sleep lifecycle with safe OTA recovery before enabling bench-sleep capability. Clock stage follows. Before integrating relevant service, settle OPEN decisions that affect it, map transition/deadline/ownership/host compatibility, verify on locked toolchain. Do not mark audit tickets closed on compile alone. The owner directs subsequent implementation tasks.

Maintain this document as decisions change. Do not rely on chat memory; read this and current source at the start of future sessions.


**2026-11-26 — LOCKED logging schema 1:** [log-contract.md](log-contract.md) and [log-schema-v1.json](log-schema-v1.json) are authoritative for Moto-Mate on-device files and records; replace previous provisional on-device CSV naming/format with compact binary and OSA legacy CSV/summary adapters. Preserve live donor functionality, no donor or protected radio changes. 24/7 monitor, multiple legs per trip, immutable file identity, exact binary widths, capped chunks, storage reserve and durable exact-hash receipt. V1.24 owner compile 918717 B / 47912 B, OTA image 919184 B app1→app0 confirmed, cold boot and /status pass, LittleFS 131072 B total/8192 used/122880 free, mount 5690/5696 us, stats 13204/13199 us; cold runtime internal heap free 262360, minimum 214196, largest 217076 B, max loop gap 73007 us, NVS 86/630 entries. V1.23 eight completed sleep entries 30007090..30007147 us, cycle 9 cancelled by physical button, Wi-Fi recovered. No V1.24 repeating sleep, log write, compression, OSA integration or power test yet. README records actual V1.05 getFlashChipMode() boot panic; do not restore that call. V1.25 implementation must follow contract and test vectors, not ad-hoc file conventions. Docs only, no firmware version bump.
