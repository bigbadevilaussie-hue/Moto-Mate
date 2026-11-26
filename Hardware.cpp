#include "Hardware.h"
#include "Config.h"
#include <driver/gpio.h>
#include <esp_timer.h>
namespace {
mm::Periodic adcSchedule(250000); // BENCH observation cadence, not crank capture
mm::DebouncedInput button(30000); // provisional debounce, no gesture policy
bool safeOutputs=false, sampled=false;
uint32_t adcMv=0;
mm::TimeUs sampleAt=0;
bool off(int pin) {
  const gpio_num_t p=static_cast<gpio_num_t>(pin);
  // Set latch before enabling output to avoid an intentional high pulse.
  const esp_err_t level=gpio_set_level(p,0);
  const esp_err_t direction=gpio_set_direction(p,GPIO_MODE_OUTPUT);
  return level==ESP_OK && direction==ESP_OK;
}
}
void hardwareBegin(mm::TimeUs now) {
  // All four outputs are active-high in the current wiring guide.
  safeOutputs=true;
  for (int pin : {MOSFET_PIN,BUZZER_PIN,TFT_BLK_PIN,RED_LED_PIN}) {
    if (!off(pin)) safeOutputs=false;
  }
  pinMode(WAKE_BTN_PIN,INPUT_PULLUP);
  button.begin(now,digitalRead(WAKE_BTN_PIN)==LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN,ADC_11db);
  adcSchedule.start(now);
  Serial.printf("[GPIO] idle output configuration %s; physical loads unverified\n",safeOutputs?"PASS":"FAIL");
}
void hardwarePoll(mm::TimeUs now) {
  button.update(now,digitalRead(WAKE_BTN_PIN)==LOW);
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
  return String(buf);
}
