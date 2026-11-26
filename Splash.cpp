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
    ok &= centered(t, modeLabel(mode), 48, 81, 148, &lgfx::fonts::FreeSansBold9pt7b);
    if (detail && *detail) ok &= centered(t, detail, 81, 109, 148, &lgfx::fonts::DejaVu12, &lgfx::fonts::DejaVu9);
  }
  ok &= centered(t, "VER " MOTO_MATE_VERSION, 110, 128, 68, &lgfx::fonts::DejaVu9);
  return ok;
}
