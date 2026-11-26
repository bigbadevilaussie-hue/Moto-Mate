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

// Fit whole strings rather than clipping them at the edges.
bool centered(lgfx::LGFXBase& t, const char* text, int y,
              float preferred, int maxWidth) {
  t.setTextDatum(lgfx::textdatum_t::top_left);
  t.setFont(&lgfx::fonts::Font0);
  t.setTextColor(WHITE);
  t.setTextSize(preferred);
  if (t.textWidth(text) > maxWidth) t.setTextSize(1);
  const int w = t.textWidth(text);
  if (w > maxWidth || y < 0 || y + t.fontHeight() > t.height()) return false;
  t.drawString(text, (160 - w) / 2, y);
  return true;
}
} // namespace

bool drawSplash(lgfx::LGFXBase& t, SplashMode mode, const char* detail,
                float voltage, float temperatureC) {
  if (t.width() != 160 || t.height() != 128) return false;
  bool ok = drawBackground(t);
  if (mode == SplashMode::BatteryCheck) {
    ok &= centered(t, modeLabel(mode), 44, 1.5f, 148);
    char v[24], temp[24];
    if (std::isfinite(voltage))
      snprintf(v, sizeof(v), "%.2f V", static_cast<double>(voltage));
    else
      snprintf(v, sizeof(v), "-- V");
    if (std::isfinite(temperatureC))
      snprintf(temp, sizeof(temp), "%.1f C", static_cast<double>(temperatureC));
    else
      snprintf(temp, sizeof(temp), "-- C");
    ok &= centered(t, v, 61, 3, 148);
    ok &= centered(t, temp, 91, 2, 148);
  } else {
    ok &= centered(t, modeLabel(mode), 58, 1.5f, 148);
    if (detail && *detail) ok &= centered(t, detail, 84, 1.5f, 148);
  }
  ok &= centered(t, "VER " MOTO_MATE_VERSION, 113, 1, 68);
  return ok;
}
