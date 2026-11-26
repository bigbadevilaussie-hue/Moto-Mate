#include "Hardware.h"
#include "Config.h"
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <initializer_list>
#include <esp_timer.h>
namespace {
mm::Periodic adcSchedule(250000); // BENCH observation cadence, not crank capture
mm::DebouncedInput button(30000); // provisional debounce, no gesture policy
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
  gpio_deep_sleep_hold_dis();
  bootCause=esp_sleep_get_wakeup_cause();
  const auto wakePin=static_cast<gpio_num_t>(WAKE_BTN_PIN);
  wakePinRtc=rtc_gpio_is_valid_gpio(wakePin);
  if (wakePinRtc && (bootCause==ESP_SLEEP_WAKEUP_EXT0 || bootCause==ESP_SLEEP_WAKEUP_EXT1))
    wakePinRtc=rtc_gpio_deinit(wakePin)==ESP_OK;
  pinMode(WAKE_BTN_PIN,INPUT_PULLUP);
  button.begin(mm::TimeUs(esp_timer_get_time()),digitalRead(WAKE_BTN_PIN)==LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN,ADC_11db);
  adcSchedule.start(now);
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
  if (adcSchedule.due(now)) {
    adcMv=analogReadMilliVolts(VOLTAGE_PIN);
    sampleAt=esp_timer_get_time();
    sampled=true;
  }
}
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
