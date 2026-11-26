#include "WifiManager.h"
#include "Config.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <cstring>

#if __has_include("Config.local.h")
#include "Config.local.h"
#endif

namespace {
const char* yn(bool b) { return b ? "PASS" : "FAIL"; }
// Extracted V1.10 BENCH Wi-Fi association path. Radio ordering and waits unchanged.
// Production BLE handover and field TX policy not yet implemented.
enum class Radio { Idle, Guard, ResetSettle, ModeSettle, InitSettle,
                   Connect, Retry, OffGuard, Done };
Radio radio = Radio::Idle;
uint32_t stageAt = 0;
uint32_t connectAt = 0;
unsigned attempt = 0;
bool wifiOK = false, offOK = false;
void stage(Radio s) { radio = s; stageAt = millis(); }
bool elapsed(uint32_t ms) { return uint32_t(millis() - stageAt) >= ms; }
bool radioOff() {
  if (sntp_enabled()) sntp_stop();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  wifi_mode_t mode = WIFI_MODE_NULL;
  const esp_err_t err = esp_wifi_get_mode(&mode);
  return err == ESP_ERR_WIFI_NOT_INIT || (err == ESP_OK && mode == WIFI_MODE_NULL);
}
void finishRadio() {
  offOK = radioOff();
  Serial.printf("[RADIO] Wi-Fi OFF: %s\n", yn(offOK));
  stage(Radio::OffGuard);
}
void failAttempt() {
  Serial.printf("[WIFI] attempt %u failed; status=%d\n", attempt, int(WiFi.status()));
  offOK = radioOff();
  if (attempt < 2 && offOK) stage(Radio::Retry);
  else finishRadio();
}
} // namespace

void wifiBegin() {
  Serial.println("\n--- 04 / WIFI -----------------------------------------");
  wifiOK = offOK = false;
  attempt = 0;
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  Serial.println("[WIFI] local credentials present (hidden); DHCP");
  Serial.println("[RADIO] BLE not started; 800 ms guard");
  stage(Radio::Guard);
#else
  Serial.println("[WIFI] FAIL: local credentials missing");
  finishRadio();
#endif
}
bool wifiPoll() {
  switch (radio) {
    case Radio::Guard:
      if (!elapsed(800)) break;
      ++attempt;
      Serial.printf("[WIFI] attempt %u/2; IDF teardown\n", attempt);
      esp_wifi_stop();
      esp_wifi_deinit();
      stage(Radio::ResetSettle);
      break;
    case Radio::ResetSettle:
      if (!elapsed(300)) break;
      if (!WiFi.mode(WIFI_STA)) { failAttempt(); break; }
      stage(Radio::ModeSettle);
      break;
    case Radio::ModeSettle: {
      if (!elapsed(100)) break;
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      esp_wifi_init(&cfg);
      esp_wifi_start();
      stage(Radio::InitSettle);
      break;
    }
    case Radio::InitSettle: {
      if (!elapsed(100)) break;
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
      const esp_err_t tx = esp_wifi_set_max_tx_power(34); // donor 8.5 dBm requested ceiling, BENCH S3 qualification
      int8_t limit = 0;
      const esp_err_t got = esp_wifi_get_max_tx_power(&limit);
      if (got == ESP_OK) Serial.printf("[WIFI] configured TX ceiling=%.2f dBm; set=%s\n", limit/4.0, esp_err_to_name(tx));
      else Serial.printf("[WIFI] TX ceiling unavailable; set=%s get=%s\n", esp_err_to_name(tx), esp_err_to_name(got));
      wifi_config_t wc = {};
      if (esp_wifi_get_config(WIFI_IF_STA, &wc) != ESP_OK) { failAttempt(); break; }
      if (strlen(WIFI_SSID) > sizeof(wc.sta.ssid) || strlen(WIFI_PASSWORD) > sizeof(wc.sta.password)) {
        Serial.println("[WIFI] FAIL: credential length"); finishRadio(); break;
      }
      memset(wc.sta.ssid, 0, sizeof(wc.sta.ssid));
      memset(wc.sta.password, 0, sizeof(wc.sta.password));
      memcpy(wc.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
      memcpy(wc.sta.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));
      memset(wc.sta.bssid, 0, sizeof(wc.sta.bssid));
      wc.sta.bssid_set = 0;
      wc.sta.pmf_cfg.required = false;
      if (esp_wifi_set_config(WIFI_IF_STA, &wc) != ESP_OK) { failAttempt(); break; }
      if (esp_wifi_connect() != ESP_OK) { failAttempt(); break; }
      connectAt = millis();
      stage(Radio::Connect);
#else
      finishRadio();
#endif
      break;
    }
    case Radio::Connect:
      if (WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) != 0) {
        wifiOK = true;
        Serial.printf("[WIFI] PASS connected in %lu ms\n", (unsigned long)(millis()-connectAt));
        Serial.printf("[WIFI] IP=%s gateway=%s MAC=%s RSSI=%d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
            WiFi.macAddress().c_str(), int(WiFi.RSSI()));
        stage(Radio::Done);
      } else if (elapsed(15000)) failAttempt();
      break;
    case Radio::Retry:
      if (elapsed(3000)) stage(Radio::Guard);
      break;
    case Radio::OffGuard:
      if (elapsed(800)) {
        stage(Radio::Done);
      }
      break;
    case Radio::Done: return true;
    case Radio::Idle: return false;
  }
  return radio == Radio::Done;
}
bool wifiConnected() { return wifiOK && WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) != 0; }
