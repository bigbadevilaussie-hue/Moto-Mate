# Moto-Mate MkII

**Battery and charging-system diagnostics for a motorcycle — without becoming the biggest drain on the battery.**

## Project Prime Directive: SAVE THE BATTERY AT ALL COSTS

Protect the battery first. Collect all useful data needed to diagnose the battery and charging system, then reliably preserve and deliver that data. Convenience, display uptime and connectivity are secondary.

- Target parked deep-sleep current: **<= 1 mA at the AGM battery input**, not merely at the ESP32. This is a design target, not a verified Moto-Mate measurement.
- Account for wake-up energy, radio activity, retries, regulators and peripheral leakage as well as sleep current.
- Collect voltage, temperature, timestamps, resting discharge trends, cranking sag, charging voltage and engine/ride transitions where the hardware and sampling permit.
- Preserve data across sleep and failed transfers. Avoid indefinite awake states and unnecessary loads. Exact protection thresholds and sampling changes remain to be agreed.
- Voltage telemetry is not a direct measurement of battery current, capacity or state of health.

## Major project rule: direct port, NOT a rewrite

Moto-Mate is the successor to [Bike-Mate](https://github.com/bigbadevilaussie-hue/Bike-Mate), which was built, installed on the bike and worked. Moto-Mate changes the hardware, not the proven software architecture.

The reuse target is approximately **95% of existing ESP32 firmware and GUI code**. The GUI is essentially a straight copy of Bike-Mate's Python/Tk application. Adapt GPIOs, display, sensors, power management and other genuinely hardware-dependent behaviour as required. Preserve existing protocols, file formats, functionality and proven workarounds. Clean and optimise the ESP32 code as it comes over: remove verified dead code, consolidate duplicates and fix evidenced faults using [TODO.md](TODO.md). No gratuitous refactoring or unauthorised changes to protected BLE/Wi-Fi code.

The reviewed donor is Bike-Mate firmware V6.49 (commit [bc5766c](https://github.com/bigbadevilaussie-hue/Bike-Mate/commit/bc5766c319da25622db1f2c86b58339afd07d5ad)); GUI v4.52 is the supplied visual reference. Check current Git HEAD before porting.

## System architecture

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

Parked five-minute wake interval, deep sleep between wakes, riding mode, rider-ready and battery-check modes, LittleFS wake/ride logs, 4 AM gzip/upload routine, BLE telemetry and Wi-Fi-only maintenance/OTA. These are intended features, not claims that the Moto-Mate firmware already implements them. Reconcile older written timing requirements with actual donor code before changing state-machine behaviour.

## Build status

**V1.00 — pre-port baseline.**

- Owner confirms test compile of all required library includes on Arduino ESP32 Core 2.0.17 passed. Original README reported approximately 300 KB flash (15%) and 19.2 KB RAM (5%).
- motomate.ino currently contains library includes and empty setup()/loop(). Config.h and an active buzzer driver exist.
- Veroboard design, custom component definitions and V4 enclosure exist.
- Firmware and GUI port not started. OSA integration not implemented for Moto-Mate.
- Runtime hardware tests, measured battery draw and complete firmware compile have not been established.

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
| [Config.h](Config.h) | GPIO assignments and firmware version |
| [motomate.ino](motomate.ino) | Current firmware skeleton |
| [moto14wires30cuts.json](moto14wires30cuts.json) | Latest committed schematic and stripboard layout |
| [veroboard.md](veroboard.md) | Wiring guide and BOM (requires reconciliation) |
| [project.md](project.md) | Hardware and operational specification (older software direction superseded) |
| [codeprt.md](codeprt.md) | Mandatory code requirements, subject to hardware reconciliation |
| [motomate_case.scad](motomate_case.scad) | V4 enclosure |

**New AI chat:** Read aihandover.md first, then check current repository state. The battery-first directive, direct-port rule and protected software requirements govern all work.
