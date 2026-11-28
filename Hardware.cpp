#include "Hardware.h"
#include "Config.h"
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <initializer_list>
#include <esp_timer.h>
namespace {
mm::Periodic adcSchedule(250000); // BENCH observation cadence, not crank capture
mm::DebouncedInput button(30000); // sole debounce owner
mm::Gestures gestures;
mm::Gesture pendingGesture=mm::Gesture::None;
bool safeOutputs=false, sampled=false, wakePinRtc=false;
esp_sleep_wakeup_cause_t bootCause=ESP_SLEEP_WAKEUP_UNDEFINED;
uint32_t adcMv=0;
mm::TimeUs sampleAt=0;
bool off(int pin) {
  const gpio_num_t p=static_cast<gpio_num_t>(pin);
  // Set latch before enabling output to avoid an intentional high pulse.
  const esp_err_t level=gpio_set_level(p,0);
  const esp_err_t direction=gpio_set_direction(p,GPIO_MODE_OUTPUT);
  // Prepare the inactive latch/direction before releasing a retained pad.
  const esp_err_t release=gpio_hold_dis(p);
  return level==ESP_OK && direction==ESP_OK && release==ESP_OK;
}
}
void hardwareBegin(mm::TimeUs now) {
  // All four outputs are active-high in the current wiring guide.
  safeOutputs=true;
  for (int pin : {MOSFET_PIN,BUZZER_PIN,TFT_BLK_PIN,RED_LED_PIN}) {
    if (!off(pin)) safeOutputs=false;
  }
  // Release global deep-sleep hold only after preparing all output pads.
  // BL is already LOW before the retained panel CS/RESET are released.
  gpio_deep_sleep_hold_dis();
  // Release retained TFT CS/RESET after the previous BENCH sleep.
  for (int p : {TFT_CS_PIN,TFT_RST_PIN})
    if (gpio_hold_dis(static_cast<gpio_num_t>(p))!=ESP_OK) safeOutputs=false;
  bootCause=esp_sleep_get_wakeup_cause();
  const auto wakePin=static_cast<gpio_num_t>(WAKE_BTN_PIN);
  wakePinRtc=rtc_gpio_is_valid_gpio(wakePin);
  if (wakePinRtc)
    wakePinRtc=rtc_gpio_deinit(wakePin)==ESP_OK;
  pinMode(WAKE_BTN_PIN,INPUT_PULLUP);
  const auto sampledAt=mm::TimeUs(esp_timer_get_time());
  button.begin(sampledAt,digitalRead(WAKE_BTN_PIN)==LOW);
  gestures.begin(sampledAt,button.value(),bootCause==ESP_SLEEP_WAKEUP_EXT0);
  pendingGesture=mm::Gesture::None;
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN,ADC_11db);
  adcSchedule.start(now);
}
bool hardwareBacklight(bool on) {
  const auto pin=static_cast<gpio_num_t>(TFT_BLK_PIN);
  if (on && !safeOutputs) return false;
  if (gpio_set_level(pin,on?1:0)!=ESP_OK) {
    Serial.println("[TFT] BL GPIO write failed");
    gpio_set_level(pin,0);
      return false;
  }
  return true;
}
void hardwareReportBoot() {
  Serial.printf("[POWER] safe_gpio=%s wake_cause=%d gpio13_rtc=%s sleep=not_armed\n",
                safeOutputs?"OK":"FAIL",int(bootCause),wakePinRtc?"OK":"FAIL");
  Serial.printf("[BUTTON] boot=%s debounce_ms=30\n",button.bootPressed()?"HELD":"RELEASED");
}
void hardwarePoll(mm::TimeUs now) {
  const uint32_t before=button.edges();
  button.update(now,digitalRead(WAKE_BTN_PIN)==LOW);
  if (button.edges()!=before) {
    Serial.printf("[BUTTON] %s edge=%lu at_us=%llu last_hold_us=%llu\n",
      button.value()?"PRESS":"RELEASE",(unsigned long)button.edges(),
      (unsigned long long)now,(unsigned long long)button.lastHoldUs());
  }
  const auto gesture=gestures.poll(now,button.value(),button.rawValue());
  if (gesture!=mm::Gesture::None) pendingGesture=gesture;
  if (adcSchedule.due(now)) {
    adcMv=analogReadMilliVolts(VOLTAGE_PIN);
    sampleAt=esp_timer_get_time();
    sampled=true;
  }
}
void hardwareGestureTiming(mm::TimeUs doubleTap, mm::TimeUs longPress) { gestures.configure(doubleTap,longPress); }
float hardwareBenchVoltage(mm::TimeUs now) {
  return sampled && now>=sampleAt && now-sampleAt<=1000000ULL ? adcMv*0.0057f : NAN;
}
mm::Gesture hardwareTakeGesture() {
  const auto result=pendingGesture;
  pendingGesture=mm::Gesture::None;
  return result;
}
bool hardwareGesturePending() { return gestures.pending() || button.rawValue(); }
String hardwareStatus() {
  char buf[360];
  if (!sampled) {
    snprintf(buf,sizeof(buf),",\"idle_gpio_config_ok\":%s,\"button_pressed\":%s,\"button_edges\":%lu,\"adc_mv\":null,\"protected_rail_estimate_mv\":null,\"voltage_qualified\":false",
      safeOutputs?"true":"false",button.value()?"true":"false",(unsigned long)button.edges());
  } else {
    snprintf(buf,sizeof(buf),",\"idle_gpio_config_ok\":%s,\"button_pressed\":%s,\"button_edges\":%lu,\"adc_mv\":%lu,\"protected_rail_estimate_mv\":%lu,\"sample_age_ms\":%llu,\"voltage_qualified\":false",
      safeOutputs?"true":"false",button.value()?"true":"false",(unsigned long)button.edges(),(unsigned long)adcMv,
      (unsigned long)((adcMv*57UL+5)/10),(unsigned long long)((esp_timer_get_time()-sampleAt)/1000));
  }
  String status(buf);
  char wake[320];
  const auto now=mm::TimeUs(esp_timer_get_time());
  snprintf(wake,sizeof(wake),
    ",\"wake_cause\":%d,\"wake_pin_rtc_ok\":%s,\"button_boot_held\":%s,\"button_raw_pressed\":%s,\"button_held_us\":%llu,\"button_last_hold_us\":%llu,\"button_released_for_wake\":%s",
    int(bootCause),wakePinRtc?"true":"false",button.bootPressed()?"true":"false",
    button.rawValue()?"true":"false",(unsigned long long)button.heldUs(now),
    (unsigned long long)button.lastHoldUs(),button.releasedForWake(now)?"true":"false");
  status+=wake;
  return status;
}

bool hardwareButtonReleased(mm::TimeUs now) {
  return digitalRead(WAKE_BTN_PIN)==HIGH && button.releasedForWake(now);
}

void hardwareCancelBenchSleep() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_deinit(static_cast<gpio_num_t>(WAKE_BTN_PIN));
  pinMode(WAKE_BTN_PIN,INPUT_PULLUP);
  gpio_deep_sleep_hold_dis();
  for (int p : {TFT_CS_PIN,TFT_RST_PIN})
    if (gpio_hold_dis(static_cast<gpio_num_t>(p))!=ESP_OK) safeOutputs=false;
  for (int p : {MOSFET_PIN,BUZZER_PIN,TFT_BLK_PIN,RED_LED_PIN})
    if (!off(p)) safeOutputs=false;
}

bool hardwareArmBenchSleep(mm::TimeUs sleepUs, bool buttonWake, bool keepBacklight) {
  const auto pin=static_cast<gpio_num_t>(WAKE_BTN_PIN);
  if (!safeOutputs || !sleepUs || (buttonWake && (!wakePinRtc || !hardwareButtonReleased(esp_timer_get_time())))) return false;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  bool ok=esp_sleep_enable_timer_wakeup(sleepUs)==ESP_OK;
  if (ok && buttonWake) ok=esp_sleep_enable_ext0_wakeup(pin,0)==ESP_OK
      && rtc_gpio_pullup_en(pin)==ESP_OK && rtc_gpio_pulldown_dis(pin)==ESP_OK;
  if (ok) {
    for (int p : {MOSFET_PIN,BUZZER_PIN,RED_LED_PIN}) {
      if (gpio_set_level(static_cast<gpio_num_t>(p),0)!=ESP_OK ||
          gpio_hold_en(static_cast<gpio_num_t>(p))!=ESP_OK) ok=false;
    }
    // BENCH-only exception: retain the already drawn sleep emoji, illuminated.
    // All recovery/FIELD callers must leave keepBacklight false.
    if (!hardwareBacklight(keepBacklight) ||
        gpio_hold_en(static_cast<gpio_num_t>(TFT_BLK_PIN))!=ESP_OK) ok=false;
  }
  // Panel stays powered on BENCH. Prevent reset or stray SPI selection.
  if (ok) {
    for (int p : {TFT_CS_PIN,TFT_RST_PIN}) {
      const auto gpio=static_cast<gpio_num_t>(p);
      if (gpio_set_level(gpio,1)!=ESP_OK || gpio_hold_en(gpio)!=ESP_OK) ok=false;
    }
  }
  if (!ok) { hardwareCancelBenchSleep(); return false; }
  gpio_deep_sleep_hold_en();
  return true;
}
