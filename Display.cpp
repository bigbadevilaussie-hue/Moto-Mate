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
lgfx::LGFX_Sprite intro(&tft);
bool ready = false;
bool drawn = false;
bool animating = false;
unsigned sleepShown=999;
SplashMode lastMode = SplashMode::Initialising;
SplashMode introMode = SplashMode::Initialising;
uint32_t introStart = 0;
uint32_t lastFrame = 0;
constexpr uint32_t INTRO_MS = 1500;
constexpr uint32_t FRAME_MS = 65;

uint16_t fadePixel(uint16_t p, unsigned level) {
  const unsigned r = ((p >> 11) * level + 127) / 255;
  const unsigned g = (((p >> 5) & 63) * level + 127) / 255;
  const unsigned b = ((p & 31) * level + 127) / 255;
  return uint16_t((r << 11) | (g << 5) | b);
}

void introFrame(uint32_t elapsed) {
  // Fade from black; a diagonal red energy sweep travels across the artwork.
  // Explicit RGB565 type avoids dependence on the panel byte-swap setting.
  const unsigned level = elapsed >= 1050 ? 255 : elapsed * 255 / 1050;
  const int sweep = int(elapsed * 220 / INTRO_MS) - 40;
  lgfx::rgb565_t strip[160 * 4];
  tft.startWrite();
  for (int y = 0; y < 128; y += 4) {
    for (int j = 0; j < 4; ++j) {
      for (int x = 0; x < 160; ++x) {
        uint16_t p = fadePixel(intro.readPixel(x, y+j), level);
        const int distance = abs(x + (y+j)/3 - sweep);
        // Restrict the sweep to the logo area, leaving dynamic text clean.
        if (y+j < 43 && distance < 12 && elapsed < 1200) {
          const unsigned boost = (12-distance) * (1200-elapsed) / 600;
          const unsigned red = (p >> 11) + boost;
          p = uint16_t((p & 0x07FF) | ((red > 31 ? 31 : red) << 11));
        }
        strip[j*160+x] = p;
      }
    }
    tft.pushImage(0, y, 160, 4, strip);
  }
  // Deterministic tiny sparks trail the sweep, then disappear completely.
  if (elapsed < 1200) {
    for (int i = 0; i < 9; ++i) {
      const int x = sweep - 6 - (i*13)%32;
      const int y = 5 + (i*17 + int(elapsed/30))%33;
      if (x > 1 && x < 158) {
        tft.drawPixel(x, y, uint16_t(0xFBE0));
        tft.drawFastHLine(x-1, y, 3, uint16_t(0xF800));
        tft.drawPixel(x, y, uint16_t(0xFFFF));
      }
    }
  }
  tft.endWrite();
}

void stopIntro() {
  if (animating) {
    intro.deleteSprite();
    animating = false;
  }
}

} // namespace

void displayBegin(bool animate) {
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
  drawn = false;
  tft.fillScreen(uint16_t(0));
  intro.setColorDepth(16);
  animating = animate && intro.createSprite(160,128) != nullptr;
  if (animating) {
    introMode = SplashMode::Initialising;
    if (!drawSplash(intro, introMode)) stopIntro();
  }
  introStart = millis();
  lastFrame = introStart - FRAME_MS;
  Serial.printf("[TFT] INIT OK 160x128 RGB565; intro=%s; BL disabled\n",
                animating ? "enabled" : "fallback");
}

void displayPoll(SplashMode mode) {
  if (!ready) return;
  sleepShown=999;
  if (animating) {
    const uint32_t now = millis();
    const uint32_t elapsed = now - introStart;
    if (elapsed < INTRO_MS) {
      if (now-lastFrame < FRAME_MS) return;
      lastFrame = now;
      if (introMode != mode) {
        introMode = mode;
        if (!drawSplash(intro, mode)) stopIntro();
      }
      if (animating) { introFrame(elapsed); return; }
    }
    if (animating && introMode == mode) {
      intro.pushSprite(0,0);
      drawn = true;
      lastMode = mode;
      stopIntro();
      Serial.println("[SPLASH] intro complete; temporary sprite released");
      return;
    }
    stopIntro();
  }
  if (drawn && mode == lastMode) return;
  drawn = true;
  lastMode = mode;
  bool ok = drawSplash(tft, mode);
  Serial.printf("[SPLASH] %s mode=%u (visual confirmation required)\n",
                ok ? "SENT" : "FAIL", unsigned(mode));
}

bool displaySleepCountdown(unsigned seconds) {
  if (!ready) return false;
  stopIntro();
  drawn=false;
  unsigned& previous=sleepShown;
  if (previous==seconds) return true;
  const uint16_t black=0x0000, yellow=0xFEC0, shade=0xD480, white=0xFFFF, blue=0x65FF;
  if (previous==999 || seconds>previous) {
    tft.fillScreen(black);
    tft.fillCircle(70,39,31,shade);
    tft.fillCircle(70,37,29,yellow);
    tft.drawLine(51,33,58,37,black);
    tft.drawLine(58,37,65,33,black);
    tft.drawLine(75,33,82,37,black);
    tft.drawLine(82,37,89,33,black);
    tft.fillCircle(70,51,7,black);
    tft.setTextDatum(lgfx::textdatum_t::top_left);
    tft.setTextColor(blue);
    tft.setTextSize(1);
    tft.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    tft.drawString("z",105,20);
    tft.drawString("Z",119,9);
    tft.setTextColor(white);
    tft.drawString("SLEEPING",(160-tft.textWidth("SLEEPING"))/2,74);
  }
  tft.fillRect(0,95,160,33,black);
  char label[20];
  if (seconds) snprintf(label,sizeof(label),"IN %u s",seconds);
  else snprintf(label,sizeof(label),"Z z z ...");
  tft.setTextColor(white);
  tft.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  tft.drawString(label,(160-tft.textWidth(label))/2,95);
  previous=seconds;
  return true;
}
void displaySleepPanel() {
  if (!ready) return;
  stopIntro();
  tft.fillScreen(uint16_t(0));
  tft.sleep();
  drawn=false;
}
void displayResumePanel() {
  if (!ready) return;
  tft.wakeup();
  drawn=false;
}

void displayBenchLive(unsigned seconds,float voltage,float temperature,float humidity) {
  if (!ready) return;
  sleepShown=999;
  stopIntro();
  static bool live=false;
  const uint16_t black=0x0000, white=0xffff, cyan=0x07ff, yellow=0xffe0;
  if (!live || drawn) { tft.fillScreen(black); live=true; drawn=false; }
  tft.fillRect(0,0,160,128,black);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextSize(1);
  tft.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  tft.setTextColor(cyan);
  tft.drawString("BENCH MODE",8,4);
  tft.drawFastHLine(8,25,144,cyan);
  char v[24],t[24],h[24],footer[32];
  if (isfinite(voltage)) snprintf(v,sizeof(v),"%.2f V*",double(voltage));
  else snprintf(v,sizeof(v),"-- V");
  if (isfinite(temperature)) snprintf(t,sizeof(t),"%.1f C",double(temperature));
  else snprintf(t,sizeof(t),"-- C");
  if (isfinite(humidity)) snprintf(h,sizeof(h),"%.0f%% RH",double(humidity));
  else snprintf(h,sizeof(h),"-- RH");
  tft.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  tft.setTextColor(white);
  tft.drawString(v,8,31);
  tft.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  tft.drawString(t,8,64);
  tft.drawString(h,86,64);
  tft.setFont(&lgfx::fonts::DejaVu9);
  tft.setTextColor(yellow);
  tft.drawString("* rail estimate / uncal",8,86);
  snprintf(footer,sizeof(footer),"SLEEP IN %us   V%s",seconds,MOTO_MATE_VERSION);
  tft.setTextColor(cyan);
  tft.drawString(footer,8,108);
}
