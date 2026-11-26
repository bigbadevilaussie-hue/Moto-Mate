# V1.13 physical TFT and selected artwork

Owner chose the first clean red/white/black Moto-Mate splash on 2026-11-26. Native 160x128 layout uses a 160x43 header and 160x18 footer from the selected generated artwork, resized to leave the central live-text area clear. The two JPEG strips are embedded as const bytes in SplashArtwork.h (3534 + 922 bytes, CRC32 208ee2ac and 147ab6e); LovyanGFX decodes to RGB565. No fixed mode or version in artwork, no persistent 40 KiB sprite. Full-resolution original remains in the chat; the native strips are preserved here. JPEG compression is an implementation choice, not a lossless raw RGB565 master.

Display.cpp configures ST7735S SPI2, write-only, 16 MHz, rotation 1, 128x160 portrait panel / 160x128 landscape. Offsets, RGB order, inversion, and orientation are provisional. BENCH only with healthy NVS at boot. One-time red, green, blue frames with white perimeter, then live Initialising/Maintenance mode based on existing coordinator. No new blocking delay in loop, no changes to Wi-Fi/OTA, no backlight-high command. Keep BL disconnected until current and drive circuit are qualified. If backlight stays dark, use a separately qualified supply/driver, not an unverified GPIO load.

Serial INIT OK/SENT means software sent commands only, not proven panel output. Owner must visually verify colour, full border, upright splash and VER 1.13. No physical readback. Build, flash and display tests pending. No field mode/sleep lifecycle or sensor-driven screen changes in this patch.

Existing seven-mode renderer remains; only Initialising and Maintenance are wired into current BENCH coordinator. Battery readings must be supplied by qualified sensors, not simulated.
