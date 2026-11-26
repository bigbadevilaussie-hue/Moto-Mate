#include "Display.h"
#include "Config.h"
#include <Arduino.h>

namespace {
class MotoDisplay : public lgfx::LGFX_Device {
  lgfx::Bus_SPI bus;
  lgfx::Panel_ST7735S panel;
public:
  MotoDisplay() {
    auto b = bus.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 16000000;
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
    // Provisional 128x160 module geometry. Verify visible border and RGB.
    p.memory_width = 128;
    p.memory_height = 160;
    p.offset_x = 0;
    p.offset_y = 0;
    p.offset_rotation = 0;
    p.readable = false;
    p.invert = false;
    p.rgb_order = true; // V1.13 owner observed red and blue reversed.
    panel.config(p);
    setPanel(&panel);
  }
};

MotoDisplay tft;
bool ready = false;
bool drawn = false;
SplashMode lastMode = SplashMode::Initialising;
uint8_t stage = 0;
uint32_t since = 0;
constexpr uint32_t TEST_MS = 1200;
constexpr uint16_t colors[] = {0xF800, 0x07E0, 0x001F};
const char* const names[] = {"RED", "GREEN", "BLUE"};

void colorTest(uint8_t index) {
  tft.fillScreen(colors[index]);
  tft.drawRect(0, 0, 160, 128, uint16_t(0xFFFF));
  tft.setFont(&lgfx::fonts::Font0);
  tft.setTextSize(2);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextColor(uint16_t(0xFFFF), uint16_t(0x0000));
  tft.drawString(names[index], (160-tft.textWidth(names[index]))/2, 56);
  since = millis();
  Serial.printf("[TFT] SENT %s; inspect colour and complete white border\n", names[index]);
}
}

void displayBegin() {
  // Hardware owns BL. Do not drive GPIO4 high until its load is qualified.
  if (!tft.init()) {
    Serial.println("[TFT] FAIL: driver init");
    return;
  }
  tft.setRotation(1);
  tft.setColorDepth(16);
  if (tft.width() != 160 || tft.height() != 128) {
    Serial.println("[TFT] FAIL: landscape dimensions");
    return;
  }
  ready = true;
  stage = 0;
  drawn = false;
  Serial.println("[TFT] INIT OK 160x128 RGB565; write-only, visual confirmation required; BL disabled");
  colorTest(stage);
}

void displayPoll(SplashMode mode) {
  if (!ready) return;
  if (stage < 3) {
    if (uint32_t(millis()-since) < TEST_MS) return;
    ++stage;
    if (stage < 3) { colorTest(stage); return; }
  }
  if (drawn && mode == lastMode) return;
  drawn = true;
  lastMode = mode;
  bool ok = drawSplash(tft, mode);
  Serial.printf("[SPLASH] %s mode=%u (visual confirmation required)\n",
                ok ? "SENT" : "FAIL", unsigned(mode));
}
