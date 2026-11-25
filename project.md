# Moto-Mate MkII — Project Specification and Lessons Learned

Updated 2026-11-25. Firmware V1.00, pre-port baseline. This document distils Bike-Mate's engineering experience for Moto-Mate. It is not a claim that the port is implemented or tested. Read [aihandover.md](aihandover.md) and [README.md](README.md) for owner direction and workflow.

## 0. PRIME DIRECTIVE — SAVE THE BATTERY AT ALL COSTS

Moto-Mate must diagnose the battery and charging system without becoming the bigger drain. Protect the ability to crank the motorcycle before prioritising telemetry, display, networking, alerts or convenience. Gather and preserve all diagnostically useful data within that power budget.

Rule #1: if it is not fun or overengineered, the owner does not want it. Rule #12: steal with pride. Neither overrides battery preservation.

**Direct port, not a rewrite.** Bike-Mate was built, installed on the motorcycle and worked; the owner wanted improved hardware. Reuse approximately 95% of the firmware and GUI. ChatGPT is technical lead for requested patches and porting. Protect the donor's tested behaviour; change only what the hardware or agreed requirements need. No code port has begun.

## 1. The actual motorcycle and why this exists

Historical reference from Bike-Mate TEST.md and DYNA_TUNE.md; confirm current battery fitment and calibration before using numerical limits.

| Item | Donor reference |
| --- | --- |
| Motorcycle | 2008 Triumph Sprint ST 1050 |
| Battery | SSB Powersport VTX12-BS AGM, 12 V, 10.5 Ah, 180 CCA; purchase recorded 7 June 2024 |
| Practical crank floor | About 12.2 V rested on this bike, not a universal AGM cutoff |
| Healthy historical observations | 12.9 V immediately after ride, 12.6 V seven days later, 14.2 V running at idle |
| Charging system | Aftermarket MOSFET regulator/rectifier fitted April 2022; 14.2–14.4 V cruise was an assumption, not measured evidence |
| Failure history | Original shunt R/R failed: running voltage fell from about 14.2 V to resting-only ~12.5 V over roughly two weeks; cranking weakened until stranded |

Diagnose the progression before the next stranding, rather than simply reporting a dead battery. A failed R/R, stator, heated connector or corroded ground can all produce low/intermittent charging voltage; a voltage log alone cannot identify the exact failed part. Overcharge is a hardware fault, not something to hide by raising an alarm threshold.

**Important limitation:** voltage-only engine detection cannot reliably identify a running engine if charging has failed and voltage never crosses the running threshold. Preserve the donor initially; evaluate rider-ready samples and the new motion sensor for evidence of this blind spot before proposing a change. Motion alone is not proof of engine operation.

## 2. Battery budget — lessons from the real Bike-Mate build

Bike-Mate README records a built and calibrated board at approximately **1 mA asleep and 77 mA awake/running**. An older paragraph estimates 250 µA and says not built; treat that as superseded design history, not a measured value. Owner confirms Bike-Mate worked on the bike. No Moto-Mate current measurement has been established.

- Target <=1 mA asleep measured at the AGM input, including buck quiescent draw, MCU regulator, sensors, indicator LEDs, TFT and leakage. This is necessary but not sufficient.
- Measure complete parked wake-cycle energy. Average current is (sleep current × sleep time + sum of awake current × awake time) / total elapsed time; integrate actual current if it varies. At a constant 1 mA, 30 days consumes 0.72 Ah, about 6.9% of the donor's nominal 10.5 Ah before self-discharge, bike loads, temperature effects or aging. Nominal capacity is not a safe usable cranking reserve.
- Shut down GPS, fan, USB charger, TFT backlight and unnecessary LEDs when not needed. Put always-on MPU into sleep. Verify GPIO hold and release across deep sleep, including boot transients.
- Preserve bounded wake, maintenance and connection behaviour. Do not extend wake time to chase a missing server or a failed upload. Log locally and retry later.
- **PANIC must remain sleep-eligible.** V4.72 specifically fixed the low-battery death spiral: don't keep the diagnostic device awake simply because battery voltage is low. Alerts must be bounded and rate-limited; never repeatedly run a high-current alarm/horn. Donor mail defaults include 3600 s rate limit and failure backoff; confirm actual latch and alert behavior when porting.
- Measure sleep, ordinary wake, disconnected wake, upload/retry, rider-ready and maintenance separately. Verify no unexpected switched-rail power or GPS phantom power. The physical kill switch cuts all power.
- Bench mode is not field mode. Latest donor defaults to bench mode; never fit an unverified configuration that stays awake for bench tests.
- Battery-saving changes must not silently discard diagnostic data. If storage fills, explicitly define retention and record loss; do not treat unacknowledged files as disposable.

### Surface charge — a critical diagnostic lesson

The observed 12.9 → 12.6 V drop in seven days was NOT all parasitic drain. Early Dyna Tune drafts incorrectly derived ~2 mA from that difference. Surface charge dissipates in the first hours after engine-off. Use comparable rested samples at least 4–6 h after engine-off, preferably overnight, and compare temperature/conditions.

Track rested voltage trends (mV/day), cranking sag and recovery, running voltage and sustained excursions. Do not report actual parasitic current, CCA, capacity or state of health from voltage alone. The donor's rough AGM voltage/SOC table is a historical heuristic, not a calibrated gauge. A real current measurement and battery test are required for direct current/capacity conclusions.

## 3. Moto-Mate hardware specification — current drawing, not old donor wiring

Current source: [moto14wires30cuts.json](moto14wires30cuts.json), reviewed blob 21112057b34e1d23c9083e0e44101b827bea7b7b. 24 × 56 holes, 14 jumpers, 30 cuts, 35 nets, 44 component instances (including off-board items). [veroboard.md](veroboard.md) is an older wiring/BOM guide; reconcile it against the drawing. Do not reuse C3 board layouts or pin numbering.

| Subsystem | Current Moto-Mate drawing |
| --- | --- |
| MCU | ESP32-S3 Super Mini HW-747 |
| Screen | 1.8-inch 128 × 160 ST7735 TFT, 3.3 V, GPIO4 direct active-high BLK |
| Environment | SHT40 temperature/humidity, 3.3 V |
| Motion | MPU-6050 GY-521, always-on 3.3 V, firmware sleep required |
| Input | AGM → kill switch → D5 1N4007 → F1 → protected rail; SA16CA TVS + C1 1000 µF |
| Always-on | MP1584EN → 5 V → D6 1N5819 → ESP 5 V; buzzer on protected 12 V |
| Switched | IRF4905 + NPN driver, gate pull-up/Zener, switched 12 V → USB charger and second MP1584EN |
| GPS/fan | 5 V switched fan; GPS after L1 10 µH + C5 100 nF + C6 470 µF |
| ADC | R1 470k / R2 100k, C2 100 nF as drawn, nominal multiplier 5.7, after D5 |
| GPS data | NEO-6M TXD → ESP RX GPIO44; ESP TX GPIO43 → R12 1k → GPS RXD; UART1 9600 |
| Indicators | Red LED, green LED follows GPIO5; active buzzer with NPN and D2 1N4148 |
| Optional RTC | U3 32.768 kHz, C3/C4 15 pF, J1 underside GPIO15/16; removal discussed, not changed |

### HW-747 board traps and fabrication lessons

- Edge header contains GPIO1–13, TX/RX and power pins; check actual physical module. GPIO14–18/21 are underside pads per original spec; GPIO19/20 are USB D+/D-. Avoid borrowing C3 pin assumptions. Confirm the HW-747 board variant and boot straps against the real board.
- 5 V pin tied to USB VBUS per original spec; verify isolation diode and safe USB programming power path. Original plan calls for removing onboard GPIO48 RGB LED to meet power budget; measure before/after rather than assuming the LED is the only drain.
- External RTC crystal is optional; internal RTC slow clock plus OSA synchronisation is the candidate simpler path. Removal has been recommended because underside soldering is difficult, but no schematic change has been approved or performed.
- Donor lesson: a 10k pull-down on C3 GPS RX GPIO2 interfered with boot and was removed. Don't transplant it. Verify UART boot output, S3 sleep/wake capabilities and GPIO hold behaviour, especially GPIO3 strap and GPIO43 TX.
- Count the real stripboard before cutting. Use the actual current schematic as the design reference. Continuity-check every cut; ensure distinct intended nets never share uncut copper. Verify rotated footprints and actual PN2222/2N2222 E/B/C and IRF4905 G/D/S pin order, not just symbol names.
- Donor V8 guide specified an inline 3 A fuse near the battery, SAE lead and kill switch. Confirm battery-side fuse protection and polarity before installation. Vibration, grommets, strain relief and enclosure heat/moisture are real reliability issues.

### Open hardware audit items — not fabrication approval

1. Independent horizontal-track connectivity reconstruction found no mapped shorts or split nets, but AGM GND pin mapped outside board (row 0, col 56 in zero-based coordinates). Verify connector footprint in editor; GPS is excluded from board placement, so separately confirm external leads.
2. D5 is 1N4007 (1 A average rating under datasheet conditions) while old BOM specifies 3 A fuse and the entire USB/device load passes through D5. Confirm current, thermal margins and suitably rated polarity protection; do not assume fuse protects diode.
3. Check charging transient/TVS clamp, capacitor voltage rating (old BOM C1 25 V), converter margins, fuse placement and real load. Do not claim automotive load-dump compliance.
4. C2 is 100 nF in drawing, 1 µF in old code spec. Resolve deliberately; check ADC settling, cranking measurement bandwidth and calibration with DMM. Account for series diode drop varying with load/temperature; do not copy C3 BATTERY_SLOPE or use a fixed diode correction blindly.
5. Confirm actual TFT BLK current/driver; no IRLZ44N in current drawing. Verify GPS off-state backfeed; 1k limits current but does not guarantee isolation. Verify I2C pull-ups/address (MPU AD0), module voltages, buzzer rating, gate Zener value and fuse value. No external I2C pull-ups drawn.
6. No physical current, wiring, regulator, thermal or continuity verification by AI. Reconcile BOM including USB charger, D2, L1, C6 and removed TFT MOSFET before ordering/fabrication.

### Current BOM summary (drawn, not purchase stock)

One ESP32-S3, one TFT, one SHT40, one MPU-6050, one NEO-6M, two MP1584EN, one IRF4905, two NPN (2N2222 specified in older guide), one dual USB-A charger, one 30 mm fan, active buzzer, red/green LEDs, rocker kill switch and wake button. Resistors: 470k ×1, 100k ×1, 1k ×3, 10k ×4, 330 Ω ×2. Capacitors: 1000 µF ×1, 470 µF ×1, 100 nF ×2, 15 pF ×2 (optional RTC). Diodes: SA16CA, D1 Zener (15 V per old spec, unlabelled in JSON), 1N4148, 1N4007, 1N5819. L1 10 µH, F1 (3 A PTC per old spec, unlabelled in JSON), optional 32.768 kHz crystal, connectors/wiring. Confirm ratings/footprints before purchase. Original spec also lists U.FL/SMA pigtail/external antenna, KF301 terminals, M3 hardware and spare modules; current purchase status unknown.

### GPIO map — Config.h is the firmware reference

| GPIO | Function |
| --- | --- |
| 1 | Voltage ADC |
| 2 | Buzzer |
| 3 | Red LED |
| 4 | TFT BLK |
| 5 | MOSFET driver and green LED |
| 6 / 7 | SDA / SCL |
| 8 / 9 / 10 / 11 / 12 | TFT RESET / DC / CS / MOSI / SCLK |
| 13 | Wake button (10k pull-up in drawing) |
| 44 / 43 | ESP GPS RX / TX |
| 15 / 16 | Optional external RTC, not in Config.h |

## 4. Development environment and protected code

2014 5K iMac, macOS Sonoma, Arduino IDE/CLI. **ESP32 Arduino core 2.0.17 ONLY** due to previous Mac/NimBLE/BLE compatibility problems. No 3.x upgrade or BLE stack migration. The all-library include test compile passed per owner; original Moto-Mate README reported ~300 KB flash and 19.2 KB RAM. Full firmware, S3 partition/FQBN, runtime and OTA tests remain outstanding. Donor uses Minimal SPIFFS and USB CDC, serial 115200; verify appropriate S3 board/partition configuration rather than copying C3 FQBN. Initial USB flash then Wi-Fi OTA maintenance intended.

**BleManager.cpp/h are protected.** Weeks of debugging. Preserve all UUIDs, framing, callbacks, ACK queues, timing, retry/connection lifecycle and host compatibility. Any necessary change must be evidenced and discussed first.

**WifiManager.cpp is protected.** C3 core 2.0.17 had failures switching from BLE to Wi-Fi. Preserve IDF stop/deinit/reinit sequence documented as proven 72/72 on Opal, raw TX power 34 (8.5 dBm), two attempts with 3 s settle, and LAN probe retries (up to four, 3 s gaps). Stop BLE, wait 500 ms before Wi-Fi as project requirement; never operate both concurrently. Opal HTTP can stall for >3 s; avoid misdiagnosing router delay as MCU failure. LAN Date header primary, NTP fallback, neither a hard gate for maintenance connectivity.

Workflow: read live files and history, confirm fault, small focused patch, firmware version bump for firmware changes, prove using specific serial line/state/endpoint. A successful compile is not functional verification. All terminal commands start with cd; local repo /Users/nick/Documents/Arduino/motomate. Never commit secrets; check Config.local.h ignore rule. See README/aihandover for further workflow rules.

## 5. Donor state machine and settings — actual V6.49, not stale prose

Source: Bike-Mate Config.h, Settings.cpp and bike_mate.ino at bc5766c. These are donor defaults, not values measured on the bike or automatically approved Moto-Mate thresholds.

| Setting | V6.49 source default |
| --- | --- |
| runningEnter_mv / runningExit_mv | 13800 / 13000 |
| runUnder_mv / runOver_mv | 13000 / 14800 |
| monitorNormal / Warning / Panic (mV) | 12500 / 12400 / 12200 |
| benchMode / sleepMin / uploadHour | true / 1 min / 4 |
| Actual wake interval function | bench 30 s; field sleepMin clamped 1–30 min |
| Engine stop debounce | 2 s (V6.49); arming 15 s |
| Rider-ready / cooldown | 45 s / 30 s; cooldown 1 Hz rows and 5 s BLE grace |
| Ride rows | 5 s |
| Field minimum awake / bench minimum awake | 5 s / 30 s in shouldSleep |
| Maintenance max / configured BLE max connection | 300 s / 600 s (inspect actual guards and exceptions) |

Retain NVS settings and proven transitions first. Donor README and TODO include older 120 s rider/cooldown, 15 s debounce proposals, stall detection, 5 min field cadence and older manual clock; they are not all current. Moto-Mate's earlier project spec proposed 13.2 V for 5 s entry, <13.0 V for 30 s exit or no motion for 5 min, 5 min parked wake, and 300 ms debounce/500 ms click window/2 s long press with single rider-ready (30 s), double battery check (5 s), triple maintenance. These are **unreconciled proposals/requirements**, not permission to replace V6.49. Decide explicitly after review and calibration. Do not confuse the donor's configured default of 1 min field sleep with its historical 5 min field target.

Preserve arming/ride close, panic recovery hysteresis, bounded rider-ready, silent cooldown (no normal BLE/display/LED/buzzer load), state flags and reason logs. Confirm new wake button GPIO uses supported S3 deep-sleep wake method and a held button cannot cause a wake storm. C3 wake API and GPIO mapping may need hardware adaptation. Keep screen updates edge-triggered; adapt rendering to TFT instead of rewriting state logic.

## 6. Clock, GPS and logging — hard-won lessons

- Latest clock is system time via settimeofday()/time(), V6.48; manual tickSeconds disabled. Earlier manual counter missed blocking time and cold-boot restore anchors mismatched (2748 s hole documented); do not resurrect them. Clock NVS restore validates 2020–2030; currentEpoch returns 0 for pre-2020. A saved epoch is not proof of elapsed time through complete power removal. OSA resync and invalid-time behavior matter.
- V5.28 older monotonic epoch floor was introduced after backward corrections caused out-of-order rows; the current currentEpoch no longer applies it. Preserve data ordering, test forward/backward corrections, and don't claim the old floor still works. Seal before a material clock correction where necessary to keep file timing coherent.
- TinyGPSPlus on Serial1, 9600, RX buffer 256 in current donor; adapt RX/TX to S3. gpsEpochUTC() still returns 0 — GPS time synchronisation is NOT implemented. Do not add it on a theory. Distinguish no fix from a valid coordinate; donor getters use zero when invalid, preserve compatibility while ensuring logs/reports do not mistake that for a real location. Inspect per-ride start location reset before porting; TODO warns of previous ride's location leaking into no-fix starts.
- Calibrate S3 voltage using actual measured ratio/ADC range and hardware. Donor Sensors.cpp uses 21-sample median and calibrated raw slope 0.007866 for the C3 divider, not reusable numbers. NTC B=3950 fix is history; replace thermistor acquisition with SHT40, preserving output semantics. New humidity/gyro fields must not break existing consumers.
- Maintain LittleFS append/open recovery and valid timestamps; do not format away data on routine boot. Wake CSV current header: epoch,lat,lon,volt,temp,state,flags,sats. Ride CSV current header: epoch,lat,lon,volt,temp,state,speed,alt,course. Old 6-column and planned 11-column specs are stale/proposals, not actual schema. Preserve metadata (# opened, # pre_ride_volt, # start_loc, # closed, upload events) and parser skipping # and header rows. Keep NVS summaries and raw logs; orphan summary recovery is an old pending issue to verify, not implemented claim.
- **Seal → reconcile → gzip → transfer → ACK → delete.** Gzip every sealed/closed file, keep raw if compression fails, remove raw only on successful compression, never compress or delete the newly opened current file. Reconcile closed orphan CSV/sealed files; V5.30 recovered storage from 53% to 15% in a documented test. Preserve normalized remote *.csv.gz naming (strip .sealed).
- BLE scheduled/manual transfer in V6.49 is gated on a live connection, avoiding wasted sealing work when OSA absent. A queued send is not a durable receipt. Keep file until explicit ACK, process queued ACKs outside BLE callback. Test lost/disconnected ACKs, filename normalization (including local .sealed.gz versus remote .gz), queue pressure and duplicate/retry behavior. Current code records upload epoch even on a disconnected attempt and clears manual trigger when connected, not necessarily after every file succeeds; verify this behavior against desired no-loss retry policy rather than assuming comments guarantee it.
- Legacy HTTP upload path still exists in DriveUpload.cpp (current Config.h UPLOAD_URL points to OSA /bike/file), including Rule 7 newest completed ride holdback in that path. Older docs describe direct GitHub upload and retirement proposals. Do not delete it or assume Rule 7 applies identically to BLE. Confirm current GUI/OSA interactions and actual scheduled 4 AM behavior.
- Historical GUI HTTP upload timeout could be a false negative because response sat in TCP buffer during blocking upload; verify actual logs/receipt before retrying or changing timeout.

## 7. OSA and GUI — preserve interfaces, not outdated architecture

OSA is required. ESP32 <-- BLE --> OSA on iMac:9999 <-- HTTP --> Python/Tk GUI. OSA owns the BLE connection; GUI polls HTTP, does not compete for BLE. Reuse donor GUI and reports as near-straight copy. Existing GUI visual reference v4.52; source bikemate/config.py at reviewed commit declares 4.54 (README says 4.58). Verify actual version before copying. Existing OSA baseline V1.33. Moto-Mate identity, storage, endpoints, upload repo and coexistence/replacement policy are open. Do not affect Fan-Mate.

- Keep UUIDs and packet semantics: DATA 26a8, TIME 26a9, STREAM 26ab, REQUEST 26ad, OTA/commands 26ae, SETTINGS 26b0, UPLOAD 26b1 (suffixes only; use actual source constants). 180-byte payload chunks, header and length checks, ACK after OSA saves. Existing OSA length check is not CRC validation; same-name/same-size duplicate handling does not prove byte equality. Treat as known limitations to test/discuss, not reasons to rewrite protected BLE. macOS GATT cache is device-name sensitive; coordinate changes only if necessary.
- HTTP maintenance: /serial, /serial-raw, /version, /settings, /seal, /upload, /clock, /maint/off, /ota, /ota-progress per donor. Preserve the working HTTP OTA path; legacy BLE URL OTA is not the preferred path. Test real .bin upload and boot version. Keep maintenance bounded and BLE off.
- OSA stores wakes/rides/dyna/bin and heartbeat logs; GUI reports list OSA files (not transient in-memory last ride). Keep local receipt separate from eventual GitHub replication. Keep raw evidence on OSA if remote upload fails.
- **Do not blindly trust HTTP 422.** Bike-Mate OSA pusher source at 434b65d still treats all 422 as already exists and saves manifest once at end, despite README TODO suggesting it was fixed. OSA_INTEGRATION.md identified loss of progress and false success. Fan-Mate's better pattern: remote tree/blob hash as truth, inspect error body, atomic manifest saves every ~10 pushes, 1.2 s throttle, coalesce triggers with Event and lock cycles. These are lessons for a future Moto-Mate pusher decision, not an instruction to modify existing OSA now.
- Donor note explicitly defers rewriting legacy pusher until volume >~50 files/cycle, non-file-exists 422, repeated mid-cycle restart or manifest >~1 MB. Preserve donor until a justified, scoped improvement; for new path don't inherit false-success logic by default.
- GUI settings dialog must remain compatible with existing NVS/JSON fields, preserve ACK success/failure, storage badge, graphs, ride finder, report reading and serial popup. Don't assume all README refactor plans shipped. DynaTune mock output is not actual diagnosis.

## 8. Dyna Tune — diagnostic lessons worth retaining (future, not port blocker)

Donor DYNA_TUNE.md v2 is a valuable design spec, **not a working analysis engine**. Current app opens mock results; dynatune_window.py exists, dynatune.py/history.py are absent at reviewed tree. Copy the working GUI first; never present placeholders as real battery health. Offload computationally expensive analysis to Mac/OSA/GUI instead of burning battery-side energy.

### Core analysis model

Read-only pure Python analysis of recorded rows and config -> structured PASS/WARN/FAIL/IDLE, metric, detail and evidence. IDLE means insufficient evidence, not good or bad. No filesystem, HTTP, BLE or Tk inside the analysis engine; separate data loading, history and presentation. Recommendations are advisory, never automatically applied. Closed loop: analyse → recommend → owner applies → measure → validate or revert.

| Check | Historical Dyna Tune proposed reference (not approved runtime thresholds) |
| --- | --- |
| charge_delta | Absolute avg running V primary: PASS >=13.8 V, WARN 13.4–13.8 V, FAIL <13.4 V. Delta over pre-ride rested V secondary: PASS >=1.3 V, WARN 1.0–1.3 V, FAIL <1.0 V. Define non-overlapping boundaries on implementation. |
| charge_high | Sustained running overvoltage, warn 14.6–14.8 V; fail >14.8 V for >5 s; immediate hardware investigation, no threshold masking. |
| crank_sag | Compare minimum against rolling median of 10 cold starts, within 0.5 V; provisional fixed bands >=10.5 V, 9.5–10.5 V, <9.5 V. Temperature/conditions matter. |
| sag_recovery | Time back within 0.2 V of average running voltage; proposed <=3 s / 3–8 s / >8 s. |
| charge_stability | Running V std dev proposed <=0.15 / 0.15–0.30 / >0.30 V. |
| under_duration / over_duration | Time outside configured operating bands; source proposals 0 / 1–30 / >30 s under, 0 / 1–10 / >10 s over. |
| end_clean | End voltage consistent with genuine engine stop vs mid-run interruption. |
| min_data | IDLE when <10 rows, <60 s span, <5 parseable voltages or >30 s discontinuity. Verify cadence and units; no false precision. |
| rest_rate / rest_runway / rest_trend | Rested comparable data >=4–6 h after ride; longer trend and runway estimates require adequate data, not surface-charge math. |

Critical sampling distinction: 5 s ride rows and ride-start delay cannot measure a short cranking minimum or 3 s recovery reliably, and cannot prove a continuous 5 s transient. Preserve useful existing capture if available; otherwise mark missing/undersampled data IDLE and discuss bounded rider-ready/cranking capture separately. Do not keep device awake indefinitely for better graphs. Do not use first ride V minus arbitrary 0.3 V as an unquestioned rested baseline; prefer genuine preceding rested sample and carry quality/source metadata. Temperature compensation and seasonal baselines are hypotheses needing validation on this motorcycle; ambient SHT40 may not equal battery temperature.

Other useful checks: clock validity, wake cadence, missing rows, upload success/latency, GPS fix rate, PANIC count, storage %, boot count, sleep balance, maintenance sessions and sensor plausibility. Donor draft storage bands <75%, 75–<90%, >=90% and clock-invalid ratio 0%, <=5%, >5% are diagnostic proposals. A single anomalous ride usually warrants review, not strong automatic conclusion; proposal uses 2 of last 3 for persistent FAIL (except overcharge) and 4+ consistent rides for strong recommendation. Don't suppress urgent genuine low-voltage evidence through this GUI policy.

Settings History: record previous/new values, timestamp, reason, recommendation ID, bike ID, transport, confirmed write result; schema-versioned files, config snapshot per ride as future work, cache age, before/after metrics. Validate against >=3 comparable post-change rides, offer deliberate revert, log revert. Firmware charge-failure alert and rested rolling log were deferred; keep battery-first timing (no unnecessary mid-ride Wi-Fi) and validate Mac-side analysis first.

Deferred diagnostic possibilities: charging dropouts (need sufficient cadence), RPM correlation (no RPM sensor in present design), temperature correlations, longer-term rested and cold-crank trends. Never fabricate data or represent missing hardware as a measured metric.

## 9. Acceptance and test plan — adapted from donor TEST.md

No boxes marked passed merely because Bike-Mate passed. Owner-confirmed baseline library compile is the only Moto-Mate compile claim.

| Gate | Required evidence |
| --- | --- |
| Hardware before battery | Count board, inspect cuts/footprints, continuity and isolation, polarity, protected/switched rail voltages, regulator/ADC range, TFT and module current; use current-limited bench supply |
| Battery budget | At input: sleep <=1 mA target, ordinary wake/OSA absent, held button, PANIC, upload failure, maintenance timeout, switched peripherals fully off; measure representative parked-cycle average |
| Sensor and display | DMM calibration across rested/charging range, SHT40 plausible temp/RH, MPU sleep/wake and orientation, TFT all screens fit, correct LEDs/buzzer, GPS outdoor fix/no fix and UART after boot |
| State machine | Bench/field selection, timed/button wake, rider-ready, simulated crank/charge/dropout/overvoltage, stop debounce, arming/cooldown, panic latch/recovery and return to sleep; test no false re-entry |
| Clock and logs | Cold boot, sleep resume, missing OSA, invalid time/backward sync, wake/ride CSV and metadata, seal/gzip failure, recovery after reset, full storage and bounded retention |
| BLE and OSA | Repeated reconnects, telemetry and time, actual durable file receipt and ACK, lost ACK/duplicate/size mismatch, disconnected scheduled upload then retry, no data loss; inspect OSA logs and actual files |
| Maintenance and OTA | BLE off → Wi-Fi on, bounded failure retries, /serial and settings, exit and engine-start behavior, real OTA version/boot, restore BLE without radio overlap |
| GUI and replication | OSA cache status stale vs connected, plots and settings, reports parse current CSVs and comments, GitHub upload actual content (not just HTTP status), no mock diagnostics advertised as real |
| Field validation | Controlled motorcycle installation after bench gates; monitor across seven days as donor test plan, record current, sleep/wake cycles, charging and rest trends, verify starting reserve. Stop if battery protection compromised. |

## 10. Enclosure and mechanical constraints

Bambu Lab A1 Mini; original material PETG-BASIC; V4 [motomate_case.scad](motomate_case.scad) comments prefer ASA for sun exposure. Verify actual stripboard dimensions/clearances and TFT mounting holes, no blind reuse of old 24×55 or C3 footprints. U-shaped tube with removable front/back caps, M3 fixings, screen/front LEDs/button/SHT40 vents, USB port, rear rocker/SAE/buzzer/fan. Check airflow, strain relief, vibration, heat and moisture protection; sensor should measure intended environment, not trapped case heat. Do not coat or block humidity sensor/connector/contact surfaces. Physical enclosure validation pending.

## 11. Sources and historical scope

All six project-authored Markdown docs in Bike-Mate main were read at [bc5766c](https://github.com/bigbadevilaussie-hue/Bike-Mate/tree/bc5766c319da25622db1f2c86b58339afd07d5ad):
- [README.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/README.md): build/operating history, constraints, workflow.
- [TODO.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/TODO.md): historic failures, fixes and deferred work; not a current backlog.
- [TEST.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/TEST.md): feature test coverage and historical pending tests.
- [DYNA_TUNE.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/DYNA_TUNE.md): vehicle history, surface charge correction, diagnostic rules and validation design.
- [OSA_INTEGRATION.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/OSA_INTEGRATION.md): pusher lessons.
- [veroboard.md](https://github.com/bigbadevilaussie-hue/Bike-Mate/blob/bc5766c319da25622db1f2c86b58339afd07d5ad/veroboard.md): V8 hardware practices.

Also inspected donor text netlists bike_mate.net / bike_mate_protel.net, Vero layout metadata in bike_mate.per and SVG labels. They show differing historical board sizes/pin arrangements; not authoritative for Moto-Mate. Cross-checked selected current donor firmware and GUI source (Config, Settings, coordinator, Sensors, GPS, WakeLogger, RideStorage, RideLogger, BleManager, WifiManager, DriveUpload, GUI config/app/reports/Dyna Tune window) and OSA pusher/ACK code at [434b65d](https://github.com/bigbadevilaussie-hue/OSA-Server/tree/434b65d6bf74734f8b4b13224472c53ad78f042e).

Review limitation: legacy binary Bike-Mate V1.10 - stripboard.pdf and bike_mate_veroboard.xlsx could not be decoded by GitHub text connector or retrieved via fallback. They are identified, not claimed read. Historical CAD/binary layout files and vendored .eggs dependency documents are not current project specifications and were not audited in full. Current Moto-Mate JSON takes precedence for physical build. New recommendations and acceptance gates above are derived engineering implications, not historical test results.

**Next action:** owner chooses first porting/hardware task. This documentation update authorizes no firmware, GUI, schematic or OSA modifications.
