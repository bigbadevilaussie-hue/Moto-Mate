# V1.13 physical TFT and selected artwork

Owner chose the first clean red/white/black Moto-Mate splash on 2026-11-26. Native 160x128 layout uses a 160x43 header and 160x18 footer from the selected generated artwork, resized to leave the central live-text area clear. The two JPEG strips are embedded as const bytes in SplashArtwork.h (3534 + 922 bytes, CRC32 208ee2ac and 147ab6e); LovyanGFX decodes to RGB565. No fixed mode or version in artwork, no persistent 40 KiB sprite. Full-resolution original remains in the chat; the native strips are preserved here. JPEG compression is an implementation choice, not a lossless raw RGB565 master.

Display.cpp configures ST7735S SPI2, write-only, 16 MHz, rotation 1, 128x160 portrait panel / 160x128 landscape. Offsets, RGB order, inversion, and orientation are provisional. BENCH only with healthy NVS at boot. One-time red, green, blue frames with white perimeter, then live Initialising/Maintenance mode based on existing coordinator. No new blocking delay in loop, no changes to Wi-Fi/OTA, no backlight-high command. Keep BL disconnected until current and drive circuit are qualified. If backlight stays dark, use a separately qualified supply/driver, not an unverified GPIO load.

Serial INIT OK/SENT means software sent commands only, not proven panel output. Owner must visually verify colour, full border, upright splash and VER 1.13. No physical readback. Build, flash and display tests pending. No field mode/sleep lifecycle or sensor-driven screen changes in this patch.

Existing seven-mode renderer remains; only Initialising and Maintenance are wired into current BENCH coordinator. Battery readings must be supplied by qualified sensors, not simulated.

## V1.14 colour-order correction

V1.13 owner compile PASS (817849 B flash, 46720 B static RAM); OTA V1.10 app1/4c134540 to V1.13 app0/5263863f PASS; NVS initialised BENCH, Wi-Fi recovered. After wiring correction, physical RGB frames and logo visible, but red displayed blue. Change only panel rgb_order false to true and bump version. V1.14 compile/OTA/visual colour verification pending. Border/offsets/orientation and BL current not yet qualified. Leave BL disconnected; no wiring, Wi-Fi or OTA changes.

## V1.15 typography

Owner confirmed V1.14 corrected physical colours and maintenance splash visible. Replaced scaled Font0 splash text with proportional FreeSansBold / DejaVu fonts from LovyanGFX 1.2.36. Fit checks cover both width and reserved vertical regions, with smaller native-font fallback; existing artwork, RGB order, display pins, Wi-Fi/OTA unchanged. V1.15 compile and visual acceptance pending. BL remains unqualified/disconnected.

## V1.16 animated intro and anti-aliased heading

Owner reported V1.15 typography only marginally better and requested an exciting fade/sparkle reveal. Remove boot RGB test sequence (colour order physically accepted V1.14). BENCH-only one-shot 1500 ms elapsed-time RGB565 pixel fade, diagonal red logo sweep and deterministic sparks. No backlight GPIO changes; no recurring animation. Temporary 40 KiB sprite is released at completion; allocation/render failure falls back to static splash. Animation is advanced from existing cooperative loop, at most one frame per call; Wi-Fi/OTA scheduling unchanged, actual frame rate subject to existing service time. Main heading rendered into temporary 2x-resolution mask (42,240 bytes), four-sample grayscale edge antialiasing; fallback to prior font if allocation or fit fails. No permanent framebuffer; transient peak about 83 KiB for both sprites, excluding allocator/driver overhead. Existing artwork unchanged. Compile, runtime, heap headroom and visual acceptance pending. BL remains disconnected/unqualified.
