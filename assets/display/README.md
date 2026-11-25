# Splash framework (V1.03)

160 x 128 landscape. The temporary vector design in Splash.cpp is an implementation of the red/white/black visual direction, not a pixel-identical copy of the generated concept and not locked final artwork. Owner approval of a native-resolution artwork asset is still pending. Keep the static background independent of dynamic text; when the bitmap is approved, replace drawBackground only. No mode or firmware version is baked into artwork.

- Header/emblem: y=0–42.
- Mode and status: y=58 and 84, white.
- Battery check: mode at y=44, live voltage at y=61, temperature at y=91, white. Invalid readings show --. Current smoke-test values are simulated, not measurements.
- Footer: y=113, version from MOTO_MATE_VERSION in Config.h, white.
- Modes: initialising, maintenance, BLE upload, Wi-Fi log upload, firmware update, rider ready, battery check. Double-click is the intended battery-check trigger; no button logic, wake timing, real sensor reads or radio activity added here.
- Renderer takes a LovyanGFX target and uses no delay, allocations of its own, hardware init or power control. Do not invoke on routine parked wakes. Production display lifecycle and dirty redraw policy are separate work.
- V1.03 extends the V1.02 off-screen smoke test, reuses its 40 KiB sprite and fixes the white-pixel comparison to RGB565 0xFFFF (LovyanGFX 1.2.36 LGFXBase::readPixel returns uint16_t).
- Compile with the previously verified core 2.0.17 and LovyanGFX 1.2.36. New build and hardware runtime are pending. Expected serial: [BOOT] MOTO-MATE V1.03; [TFT] PASS: 160x128 RGB565 sprite and pixel readback; [SPLASH] PASS: 7 modes and invalid-reading placeholders.
