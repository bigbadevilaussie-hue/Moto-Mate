#include "Splash.h"
#include "Config.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t RED = 0xE8A2;
constexpr uint16_t SILVER = 0xCE79;
constexpr uint16_t DARK_RED = 0x3800;

// Temporary vector artwork based on the agreed visual layout.
// Keep static artwork separate from live text. Replace only this function
// with the approved native-resolution bitmap after artwork sign-off.
void drawBackground(lgfx::LGFXBase& t) {
  t.fillScreen(BLACK);
  t.drawLine(0, 24, 24, 0, RED);
  t.drawLine(0, 30, 30, 0, DARK_RED);
  t.drawLine(0, 107, 20, 127, RED);
  t.drawLine(159, 107, 139, 127, RED);
  t.drawLine(159, 0, 149, 10, DARK_RED);
  // Gear and lightning emblem.
  t.fillCircle(18, 20, 10, SILVER);
  t.fillRect(15, 7, 6, 26, SILVER);
  t.fillRect(5, 17, 26, 6, SILVER);
  t.fillCircle(18, 20, 6, BLACK);
  t.fillTriangle(20, 8, 10, 22, 19, 22, RED);
  t.fillTriangle(17, 18, 26, 18, 15, 32, RED);
  t.setTextDatum(lgfx::textdatum_t::top_left);
  t.setFont(&lgfx::fonts::Font0);
  t.setTextSize(2);
  t.setTextColor(WHITE);
  t.drawString("Moto-", 35, 12);
  t.setTextColor(RED);
  t.drawString("Mate", 35 + t.textWidth("Moto-"), 12);
  t.drawLine(43, 8, 81, 3, RED);
  t.drawLine(81, 3, 116, 8, RED);
  // ECG underline; keep central and footer regions clear for live text.
  t.drawFastHLine(8, 36, 117, RED);
  t.drawLine(124, 36, 130, 30, RED);
  t.drawLine(130, 30, 134, 42, RED);
  t.drawLine(134, 42, 139, 27, RED);
  t.drawLine(139, 27, 144, 36, RED);
  t.drawFastHLine(144, 36, 10, RED);
  t.drawFastHLine(16, 116, 28, RED);
  t.drawFastHLine(116, 116, 28, RED);
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
  drawBackground(t);

  bool ok = true;
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
