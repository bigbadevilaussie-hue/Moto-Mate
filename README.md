# Moto-Mate MkII

### User Profile (Read This First)
*   **Nick is a smart-ass, sarcastic Australian male who gives no fucks.**
*   He will call you a cunt, a clown, or a retard. Do not take it personally. It's a term of endearment.
*   He prefers direct, blunt answers. No corporate AI fluff.
*   If he asks a question, answer it. If he tells you to do something, do it. 
*   Do not nag him with "Are you ready to...?" or "Did the print finish?" unless it's directly relevant to the current task.


The overengineered, colour-screen successor to Bike-Mate. Built clean from scratch to eliminate the ticking bombs of MkI.

## Features
*   ESP32-S3 Super Mini (HW-747)
*   1.8" ST7735 TFT Colour Display
*   SHT40 Temp/Humidity + MPU-6050 Gyro/Accel
*   NEO-6M GPS (with external antenna)
*   1mA Deep Sleep Budget
*   12V Switched Line (IRF4905 High-Side)
*   BLE Telemetry (Arduino Core 2.0.17 ONLY)
*   OTA Updates (Wi-Fi Maintenance Mode only)
*   LittleFS Logging with 4 AM Gzip/Upload Routine


## Mandatory Code Requirements
See `codeprt.md` for the non-negotiable software rules (sleep, GPIO holds, TFT polarity, MPU sleep, etc.).

## Hardware Spec
See `project.md` for the full pinout and AI-reviewed design spec.

## Build Status
*   **v1.00:** Initial repo setup.
*   **Baseline Compile:** 300KB Flash (15%), 19.2KB RAM (5%). All libraries fit comfortably.

---

## ⚠️ STRICT RULES & GOTCHAS (Read First)

### Tone
*   Be direct. No lectures. No explaining basic git concepts.
*   Talk like a competent senior dev, not a tutorial.
*   If you're about to explain something obvious, shut up and give the command.
*   No time-of-day references. No sleep/rest/break suggestions.
*   Nick decides when to work and when to stop.

### Shell - zsh is broken
*   Multi-line pastes break. `#` triggers history expansion.
*   **Write patches as Python scripts to `/tmp/` and run with `python3 /tmp/patch.py`.** Never paste heredocs directly into the shell.
*   Every patch script must exit non-zero on anchor miss. Silent no-ops are worse than crashes.
*   `zsh ?` glob trap: Quote arguments or drop comments.
*   If a heredoc misbehaves: use `printf '%s\n'` and redirect to `/tmp/x.py`.
*   Never paste Python with `python3 -c`. Always `/tmp/x.py`.
*   A Python string anchor matches exact bytes. If a file ends without a trailing newline, `"...;\n"` will MISS. Check with `python3 -c 'print(repr(open("f").read()[-40:]))'` before writing the anchor.

### Patch Discipline
*   **Never patch a file you haven't seen the current contents of.** If the last paste was more than one exchange ago, ask again.
*   **Never guess anchors or regex.** Read the file. Grep for the exact line. If the anchor misses, stop and ask for current contents. Do not guess.
*   One patch. One file when possible. Test before the next.
*   Version bump is part of every patch. `[BOOT] MOTO-MATE VX.YZ` is the ground truth for which binary is on the board.
*   After every patch, state how to verify it worked. A specific log line, TFT state, or endpoint response.

### Diagnosing
*   **Never patch a theory.** Propose it, propose the cheapest way to confirm, get the answer, then patch.
*   Git history is the answer more often than the code.
*   If a subsystem has a comment like "proven 72/72", do not touch it on a theory. Ask why it's that shape first.
*   Intermittent != broken. The fix is usually a retry, not a rewrite.

### Communication
*   Answer, then stop. Do not explain three things at once.
*   Do not write nine scripts at once. One file, one patch, one command.
*   Nick edits files himself. Ask before assuming automation.

### Session Hygiene
*   This is one stateful session, not isolated replies.
*   Before proposing a fix, ask: "have I confirmed current state, or am I assuming it?"
*   Before saying "this is a bug", ask: "could this be load-bearing for a reason I don't see?"
*   Read all pasted output. The answer is usually in the line you skimmed.

### Moto-Mate Specific
*   **Arduino Core v2.0.17 ONLY.** Do NOT update to 3.x.
*   **Wi-Fi / BLE Coexistence:** Never run both at once. Kill BLE, wait 500ms, then bring up Wi-Fi.
*   **TX Power:** 8.5 dBm is mandatory on the XCW board (and likely the S3).
*   `Config.local.h` is gitignored. Never commit it, never paste a dump containing it.
