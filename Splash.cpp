#include "Splash.h"
#include "Config.h"
#include "SplashArtwork.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;

// The static native-size artwork lives in flash; mode and version are live.
bool drawBackground(lgfx::LGFXBase& t) {
  t.fillScreen(BLACK);
  const bool top = t.drawJpg(MOTO_HEADER_JPG, sizeof(MOTO_HEADER_JPG), 0, 0);
  const bool bottom = t.drawJpg(MOTO_FOOTER_JPG, sizeof(MOTO_FOOTER_JPG), 0, 110);
  return top && bottom;
}

const char* modeLabel(SplashMode mode) {
  switch (mode) {
    case SplashMode::Initialising: return "INITIALISING";
    case SplashMode::Maintenance: return "MAINTENANCE MODE";
    case SplashMode::BleUpload: return "BLE UPLOADING";
    case SplashMode::LogUpload: return "UPLOADING LOGS";
    case SplashMode::FirmwareUpdate: return "FIRMWARE UPDATE";
    case SplashMode::RiderReady: return "RIDER READY";
    case SplashMode::BatteryCheck: return "BATTERY CHECK";
  }
  return "UNKNOWN MODE";
}

// Use native-size proportional glyphs, not enlarged 5x7 pixel text.
// Fit each string into its own reserved region without touching the artwork.
bool centered(lgfx::LGFXBase& t, const char* text, int top, int bottom,
              int maxWidth, const lgfx::IFont* preferred,
              const lgfx::IFont* fallback = &lgfx::fonts::DejaVu12) {
  if (top < 0 || bottom > t.height() || top >= bottom) return false;
  t.setTextDatum(lgfx::textdatum_t::top_left);
  t.setTextColor(WHITE);
  t.setTextSize(1);
  t.setFont(preferred);
  if (t.textWidth(text) > maxWidth || t.fontHeight() > bottom-top)
    t.setFont(fallback);
  const int w = t.textWidth(text);
  const int h = t.fontHeight();
  if (w > maxWidth || h > bottom-top) return false;
  t.drawString(text, (160-w)/2, top+(bottom-top-h)/2);
  return true;
}
// Render the main heading at 2x resolution then average four coverage
// samples into each physical pixel. The central region has a black background.
bool smoothMode(lgfx::LGFXBase& t, const char* label) {
  constexpr int W = 160, H = 33, TOP = 48;
  lgfx::LGFX_Sprite mask;
  mask.setColorDepth(16);
  if (!mask.createSprite(W*2,H*2)) return false;
  mask.fillScreen(BLACK);
  mask.setTextDatum(lgfx::textdatum_t::top_left);
  mask.setTextColor(WHITE);
  mask.setTextSize(1);
  mask.setFont(&lgfx::fonts::FreeSansBold18pt7b);
  if (mask.textWidth(label) > 296 || mask.fontHeight() > H*2)
    mask.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  const int w = mask.textWidth(label), h = mask.fontHeight();
  if (w > 296 || h > H*2) { mask.deleteSprite(); return false; }
  mask.drawString(label, (W*2-w)/2, (H*2-h)/2);
  lgfx::rgb565_t row[W];
  t.startWrite();
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      unsigned sum = 0;
      for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i)
          sum += mask.readPixel(x*2+i,y*2+j) != BLACK;
      const uint16_t shade = uint16_t(((sum*31/4)<<11) | ((sum*63/4)<<5) | (sum*31/4));
      row[x] = shade;
    }
    t.pushImage(0,TOP+y,W,1,row);
  }
  t.endWrite();
  mask.deleteSprite();
  return true;
}
} // namespace

bool drawSplash(lgfx::LGFXBase& t, SplashMode mode, const char* detail,
                float voltage, float temperatureC) {
  if (t.width() != 160 || t.height() != 128) return false;
  bool ok = drawBackground(t);
  if (mode == SplashMode::BatteryCheck) {
    ok &= centered(t, modeLabel(mode), 43, 61, 148, &lgfx::fonts::DejaVu12);
    char v[24], temp[24];
    if (std::isfinite(voltage))
      snprintf(v, sizeof(v), "%.2f V", static_cast<double>(voltage));
    else
      snprintf(v, sizeof(v), "-- V");
    if (std::isfinite(temperatureC))
      snprintf(temp, sizeof(temp), "%.1f C", static_cast<double>(temperatureC));
    else
      snprintf(temp, sizeof(temp), "-- C");
    ok &= centered(t, v, 61, 91, 148, &lgfx::fonts::FreeSansBold12pt7b, &lgfx::fonts::FreeSansBold9pt7b);
    ok &= centered(t, temp, 91, 110, 148, &lgfx::fonts::FreeSansBold9pt7b);
  } else {
    if (!smoothMode(t, modeLabel(mode)))
      ok &= centered(t, modeLabel(mode), 48, 81, 148, &lgfx::fonts::FreeSansBold9pt7b);
    if (detail && *detail) ok &= centered(t, detail, 81, 109, 148, &lgfx::fonts::DejaVu12, &lgfx::fonts::DejaVu9);
  }
  ok &= centered(t, "VER " MOTO_MATE_VERSION, 110, 128, 68, &lgfx::fonts::DejaVu9);
  return ok;
}
