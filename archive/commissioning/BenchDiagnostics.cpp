#include "BenchDiagnostics.h"
#include "Config.h"
#include "Splash.h"
#include <LovyanGFX.hpp>

// Temporary display smoke test. No TFT or backlight is initialised.
class MotoDisplay : public lgfx::LGFX_Device {
  lgfx::Bus_SPI bus;
  lgfx::Panel_ST7735S panel;

public:
  MotoDisplay() {
    auto b = bus.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 16000000;  // provisional bench setting
    b.pin_sclk = TFT_SCLK_PIN;
    b.pin_mosi = TFT_MOSI_PIN;
    b.pin_miso = -1;
    b.pin_dc = TFT_DC_PIN;
    bus.config(b);
    panel.setBus(&bus);

    auto p = panel.config();
    p.pin_cs = TFT_CS_PIN;
    p.pin_rst = TFT_RST_PIN;
    p.pin_busy = -1;
    p.panel_width = 128;
    p.panel_height = 160;
    // Offsets, inversion and colour order require physical panel validation.
    p.readable = false;
    panel.config(p);
    setPanel(&panel);
  }
};

static MotoDisplay display;

bool runBenchDisplayDiagnostics() {
  lgfx::LGFX_Sprite frame(&display);
  frame.setColorDepth(16);
  if (!frame.createSprite(160, 128)) {
    Serial.println("[TFT] FAIL: sprite allocation");
    return false;
  }

  frame.fillScreen(uint16_t(0x0000));
  frame.fillRect(0, 0, 160, 64, uint16_t(0x051F));
  frame.fillRect(0, 64, 160, 64, uint16_t(0x9A60));
  frame.drawLine(12, 82, 148, 46, uint16_t(0xFFFF));
  frame.drawCircle(80, 64, 28, uint16_t(0xFFFF));
  frame.setTextColor(uint16_t(0xFFFF));
  frame.setTextSize(2);
  frame.drawString("MOTO-MATE", 24, 8);
  frame.drawString("14.2V", 48, 100);

  // Known pixels in the RAM sprite, not physical panel readback.
  frame.drawPixel(0, 0, uint16_t(0xFFFF));
  frame.drawPixel(159, 127, uint16_t(0x0000));
  const bool ok = frame.width() == 160 && frame.height() == 128
               && frame.readPixel(0, 0) == 0xFFFF
               && frame.readPixel(159, 127) == 0x000000;
  Serial.println(ok ? "[TFT] PASS: 160x128 RGB565 sprite and pixel readback"
                    : "[TFT] FAIL: sprite dimensions or pixel readback");

  const SplashMode modes[] = {
    SplashMode::Initialising, SplashMode::Maintenance,
    SplashMode::BleUpload, SplashMode::LogUpload,
    SplashMode::FirmwareUpdate, SplashMode::RiderReady,
    SplashMode::BatteryCheck
  };
  bool splashOk = true;
  for (auto mode : modes) {
    // Synthetic values only. No sensors or radio are enabled.
    splashOk &= drawSplash(frame, mode, "TEST", 12.60f, 24.5f);
    splashOk &= frame.readPixel(8, 36) == uint16_t(0xE8A2);
  }
  splashOk &= drawSplash(frame, SplashMode::BatteryCheck);
  frame.deleteSprite();
  Serial.println(splashOk ? "[SPLASH] PASS: 7 modes and invalid-reading placeholders"
                          : "[SPLASH] FAIL: renderer fit or background pixel");
  return ok && splashOk;
}
