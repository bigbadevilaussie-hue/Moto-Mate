#include "BenchCommission.h"
#include "Config.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <cstring>

#if __has_include("Config.local.h")
#include "Config.local.h"
#endif

namespace {
const char* yn(bool b) { return b ? "PASS" : "FAIL"; }
const esp_partition_t* fsPart() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
}
bool blankPartition(const esp_partition_t* p) {
  if (!p || p->encrypted) return false;
  uint8_t bytes[1024];
  for (size_t pos = 0; pos < p->size; pos += sizeof(bytes)) {
    const size_t n = p->size - pos < sizeof(bytes) ? p->size - pos : sizeof(bytes);
    if (esp_partition_read(p, pos, bytes, n) != ESP_OK) return false;
    for (size_t i = 0; i < n; ++i) if (bytes[i] != 0xff) return false;
    yield();
  }
  return true;
}

bool storageProbe() {
  const char* path = "/.mm-commission-test";
  if (LittleFS.exists(path)) {
    Serial.println("[FS] FAIL: probe path exists; preserved");
    return false;
  }
  const uint8_t payload[] = {0x4d,0x4d,0x01,0x05,0x00,0xff,0x55,0xaa};
  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("[FS] FAIL: probe open");
    return false;
  }
  bool wrote = f.write(payload, sizeof(payload)) == sizeof(payload);
  f.close();
  f = LittleFS.open(path, FILE_READ);
  uint8_t readback[sizeof(payload)] = {};
  const bool read = f && f.size() == sizeof(payload) &&
      f.read(readback, sizeof(readback)) == sizeof(readback) &&
      memcmp(payload, readback, sizeof(payload)) == 0;
  f.close();
  const bool removed = LittleFS.remove(path);
  Serial.printf("[FS] write=%s readback=%s delete=%s\n", yn(wrote), yn(read), yn(removed));
  return wrote && read && removed;
}

// Bench-only adaptation of the donor's IDF teardown/re-init ordering.
// Production BleManager/WifiManager remain unported and untouched.
enum class Radio { Idle, Guard, ResetSettle, ModeSettle, InitSettle,
                   Connect, Retry, ClockLan, Ntp, OffGuard, Done };
Radio radio = Radio::Idle;
uint32_t stageAt = 0;
uint32_t connectAt = 0;
unsigned attempt = 0;
bool wifiOK = false, clockOK = false, offOK = false;
bool keepConnected = false;
void stage(Radio s) { radio = s; stageAt = millis(); }
bool elapsed(uint32_t ms) { return uint32_t(millis() - stageAt) >= ms; }
bool validEpoch(time_t t) { return t >= 1700000000LL && t <= 1893456000LL; }

void clockReport(const char* source) {
  time_t t = time(nullptr);
  struct tm utc = {};
  char date[32] = {};
  if (validEpoch(t) && gmtime_r(&t, &utc))
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S UTC", &utc);
  Serial.printf("[CLOCK] %s source=%s epoch=%lld %s\n",
      yn(clockOK), source, static_cast<long long>(t), date);
}

// Same LAN Date-header clock source as Bike-Mate; UTC conversion without TZ mutation.
time_t utcEpoch(const struct tm& tmv) {
  int y = tmv.tm_year + 1900;
  unsigned m = tmv.tm_mon + 1, d = tmv.tm_mday;
  if (y < 2023 || y > 2030 || m < 1 || m > 12 || d < 1 || d > 31 ||
      tmv.tm_hour < 0 || tmv.tm_hour > 23 || tmv.tm_min < 0 || tmv.tm_min > 59 ||
      tmv.tm_sec < 0 || tmv.tm_sec > 59) return 0;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const time_t t = time_t((static_cast<long long>(era) * 146097 + doe - 719468) * 86400LL +
                         tmv.tm_hour * 3600LL + tmv.tm_min * 60LL + tmv.tm_sec);
  struct tm check = {};
  if (!gmtime_r(&t, &check) || check.tm_year != tmv.tm_year ||
      check.tm_mon != tmv.tm_mon || check.tm_mday != tmv.tm_mday) return 0;
  return t;
}
bool lanClock() {
  if (uint32_t(WiFi.gatewayIP()) == 0) return false;
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  http.setReuse(false);
  String url = "http://" + WiFi.gatewayIP().toString() + "/";
  if (!http.begin(url)) return false;
  const char* keys[] = {"Date"};
  http.collectHeaders(keys, 1);
  int code = http.GET();
  String date = http.header("Date");
  http.end();
  Serial.printf("[CLOCK] LAN HTTP=%d\n", code);
  if (code <= 0) return false;
  struct tm tmv = {};
  const char* end = strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tmv);
  if (!end || *end) return false;
  const time_t t = utcEpoch(tmv);
  if (!validEpoch(t)) return false;
  struct timeval tv = {};
  tv.tv_sec = t;
  return settimeofday(&tv, nullptr) == 0;
}

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

void commissionBanner(int wakeCause) {
  Serial.println();
  Serial.println("+------------------------------------------------------+");
  Serial.println("|                                                      |");
  Serial.println("|   M O T O - M A T E     /\\_/\\   DIAGNOSTIC SYSTEM   |");
  Serial.println("|                                                      |");
  Serial.println("|   ESP32-S3  /  BENCH COMMISSIONING                   |");
  Serial.println("|   FIRMWARE  V" MOTO_MATE_VERSION "                                    |");
  Serial.println("|   SAVE THE BATTERY AT ALL COSTS                     |");
  Serial.println("|                                                      |");
  Serial.println("+------------------------------------------------------+");
  Serial.println("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION);
  Serial.println("\n--- 01 / HARDWARE -------------------------------------");
  Serial.printf("[CPU] %s rev=%u cores=%u MHz=%u\n", ESP.getChipModel(),
      unsigned(ESP.getChipRevision()), unsigned(ESP.getChipCores()), unsigned(getCpuFrequencyMhz()));
  Serial.printf("[RESET] reason=%d wake=%d\n", int(esp_reset_reason()), wakeCause);
  Serial.printf("[HEAP] total=%lu free=%lu min_free=%lu\n", (unsigned long)ESP.getHeapSize(),
      (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
  Serial.printf("[PSRAM] runtime available=%lu bytes (not physical capacity)\n", (unsigned long)ESP.getPsramSize());
  Serial.println("[GPIO] external hardware NOT TESTED; no outputs driven");
}

bool commissionStorage() {
  Serial.println("\n--- 02 / FLASH & OTA ----------------------------------");
  // Core 2.0.17 getFlashChipMode() dereferences an invalid register on this S3
  // (owner-decoded V1.05 panic at Esp.cpp:356). Avoid that API entirely.
  Serial.printf("[FLASH] physical=%lu bytes\n", (unsigned long)ESP.getFlashChipSize());
  Serial.printf("[FLASH] speed=%lu Hz; mode not queried (core 2.0.17)\n",
      (unsigned long)ESP.getFlashChipSpeed());
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* update = esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* part = fsPart();
  const unsigned long app = ESP.getSketchSize();
  if (running) Serial.printf("[OTA] running=%s offset=0x%lx capacity=%lu\n", running->label,
      (unsigned long)running->address, (unsigned long)running->size);
  if (update) Serial.printf("[OTA] update=%s offset=0x%lx capacity=%lu\n", update->label,
      (unsigned long)update->address, (unsigned long)update->size);
  Serial.printf("[APP] image=%lu bytes\n", app);
  const bool layoutOK = running && update && running->address != update->address &&
      app > 0 && app <= running->size && app <= update->size && part;
  if (running && app <= running->size)
    Serial.printf("[APP] running slot headroom=%lu bytes\n", (unsigned long)(running->size-app));
  Serial.printf("[PART] dual OTA + LittleFS partition: %s\n", yn(layoutOK));
  Serial.println("\n--- 03 / LITTLEFS -------------------------------------");
  if (!part) { Serial.println("[FS] FAIL: spiffs partition missing"); return false; }
  Serial.printf("[FS] label=%s offset=0x%lx partition=%lu bytes\n", part->label,
      (unsigned long)part->address, (unsigned long)part->size);
  if (!LittleFS.begin(false)) {
    // Only the entire erased, unencrypted partition authorises first provisioning.
    // Never format a nonblank/unknown filesystem after a mount error.
    if (!blankPartition(part)) {
      Serial.println("[FS] FAIL: nonblank or unreadable partition; NO FORMAT");
      return false;
    }
    Serial.println("[FS] verified erased partition; initialising LittleFS");
    if (!LittleFS.format() || !LittleFS.begin(false)) {
      Serial.println("[FS] FAIL: initialisation"); return false;
    }
  }
  Serial.println("[FS] mount PASS");
  const bool probeOK = storageProbe();
  const size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  const bool capacityOK = total > 0 && used <= total && total <= part->size;
  if (capacityOK)
    Serial.printf("[FS] usable=%lu used=%lu free=%lu utilisation=%.2f%%\n",
        (unsigned long)total, (unsigned long)used, (unsigned long)(total-used), 100.0 * used / total);
  else Serial.println("[FS] FAIL: invalid capacity");
  File root = LittleFS.open("/");
  bool directoryOK = root && root.isDirectory();
  unsigned count = 0, dirs = 0;
  if (directoryOK) {
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
      if (f.isDirectory()) ++dirs; else ++count;
      f.close();
      yield();
    }
    Serial.printf("[FS] root files=%u directories=%u\n", count, dirs);
  }
  root.close();
  Serial.println("[FS] wake/ride/upload queues: NOT PORTED");
  LittleFS.end();
  const bool ok = layoutOK && capacityOK && probeOK && directoryOK;
  Serial.printf("[STORAGE] %s\n", yn(ok));
  return ok;
}

void commissionRadioBegin(bool keepWifi) {
  keepConnected = keepWifi;
  Serial.println("\n--- 04 / WIFI -----------------------------------------");
  wifiOK = clockOK = offOK = false;
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
bool commissionRadioPoll() {
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
      const esp_err_t tx = esp_wifi_set_max_tx_power(80); // requested 20 dBm ceiling, BENCH only
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
        if (keepConnected) stage(Radio::Done);
        else {
          Serial.println("\n--- 05 / CLOCK ----------------------------------------");
          stage(Radio::ClockLan);
        }
      } else if (elapsed(15000)) failAttempt();
      break;
    case Radio::Retry:
      if (elapsed(3000)) stage(Radio::Guard);
      break;
    case Radio::ClockLan:
      clockOK = lanClock();
      if (clockOK) { clockReport("Opal LAN Date"); finishRadio(); }
      else {
        Serial.println("[CLOCK] LAN unavailable; NTP fallback");
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        stage(Radio::Ntp);
      }
      break;
    case Radio::Ntp:
      if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && validEpoch(time(nullptr))) {
        clockOK = true; clockReport("NTP"); finishRadio();
      } else if (elapsed(20000)) {
        clockReport("no verified sync"); finishRadio();
      }
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
bool commissionRadioPassed() { return wifiOK && clockOK && offOK; }
bool commissionWifiConnected() { return wifiOK && WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) != 0; }
void commissionRadioCancel() {
  if (radio != Radio::Idle && radio != Radio::Done) {
    offOK = radioOff();
    Serial.printf("[RADIO] abort; Wi-Fi OFF: %s\n", yn(offOK));
    stage(Radio::Done);
  }
}
