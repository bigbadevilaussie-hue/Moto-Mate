#include "BenchSleep.h"
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_attr.h>
#include "Config.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>
#include <atomic>
#include <cstring>

namespace {
constexpr char NAME[] = "Moto-Mate-B2";
constexpr char SERVICE[] = "0da80001-7d91-4a75-9a7c-56d1f29b0731";
constexpr char IDENTITY[] = "0da80002-7d91-4a75-9a7c-56d1f29b0731";
constexpr char CHALLENGE[] = "0da80003-7d91-4a75-9a7c-56d1f29b0731";
constexpr char CLOCK[] = "0da80004-7d91-4a75-9a7c-56d1f29b0731";
constexpr char METRICS[] = "0da80005-7d91-4a75-9a7c-56d1f29b0731";
// Leave teardown margin; synchronous SDK calls can still overrun.
constexpr uint32_t STOP_US = 7000000;
constexpr uint32_t TARGET_US = 10000000;
constexpr uint64_t SLEEP_US = 300000000ULL;
constexpr uint32_t MAGIC = 0x4d4d0108;
constexpr uint32_t MAX_WAKES = 3;
struct Retained {
  uint32_t magic, run, wake, priorAwakeUs, priorFlags;
};
RTC_DATA_ATTR Retained retained = {};
uint32_t wakeCause = 0;
bool sleepArmed = false;
bool offVerified = false;
std::atomic<bool> connected{false}, challengeOK{false}, clockOK{false};
std::atomic<bool> disconnected{false};
std::atomic<unsigned> completed{0};
std::atomic<uint32_t> setEpoch{0}, connAt{0}, discAt{0};
bool active = false, done = false, passed = false;
uint32_t started = 0;
BLEAdvertising* advertising = nullptr;

uint32_t readLE(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
      (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
void writeLE(uint8_t* p, uint32_t n) {
  for (unsigned i=0; i<4; ++i) p[i] = uint8_t(n >> (8*i));
}
class ServerEvents : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    challengeOK = false;
    clockOK = false;
    setEpoch = 0;
    connAt = millis();
    connected = true;
    Serial.printf("[BLE] connected at %lu ms\n", (unsigned long)(millis()-started));
  }
  void onDisconnect(BLEServer*) override {
    if (challengeOK.load() && clockOK.load()) ++completed;
    connected = false;
    discAt = millis();
    disconnected = true;
    Serial.printf("[BLE] disconnect; verified writes=%u session_ms=%lu\n",
        completed.load(), (unsigned long)(millis()-connAt.load()));
  }
};
class ChallengeEvents : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (!connected.load() || v.size()!=8) return;
    c->setValue(reinterpret_cast<uint8_t*>(const_cast<char*>(v.data())), v.size());
    c->notify();
    challengeOK = true;
    Serial.println("[BLE] challenge notification submitted");
  }
};
class ClockEvents : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (!connected.load() || v.size()!=4 || !challengeOK.load()) return;
    const uint32_t epoch = readLE(reinterpret_cast<const uint8_t*>(v.data()));
    if (epoch < 1700000000UL || epoch > 1893456000UL) return;
    struct timeval tv = {};
    tv.tv_sec = epoch;
    if (settimeofday(&tv, nullptr)==0) {
      setEpoch = epoch;
      clockOK = true;
      Serial.printf("[BLE] clock applied epoch=%lu\n", (unsigned long)epoch);
    }
  }
  void onRead(BLECharacteristic* c) override {
    uint8_t v[8] = {};
    writeLE(v, uint32_t(time(nullptr)));
    writeLE(v+4, setEpoch.load());
    c->setValue(v, sizeof(v));
  }
};
ServerEvents serverEvents;
ChallengeEvents challengeEvents;
ClockEvents clockEvents;

void finish(bool handshake) {
  if (!active) return;
  active = false;
  if (advertising) advertising->stop();
  BLEDevice::deinit(false);
  advertising = nullptr;
  const bool off = esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE &&
      esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED;
  offVerified = off;
  passed = handshake && off;
  done = true;
  Serial.printf("[BLE] local sessions=%u elapsed_ms=%lu shutdown=%s result=%s\n",
      completed.load(), (unsigned long)(millis()-started),
      off?"PASS":"FAIL", passed?"PASS":"FAIL");
  Serial.println("[OSA] verify host challenge/clock results independently");
}
}

static void beginRadio() {
  started = millis();
  active = true;
  done = passed = false;
  completed = 0;
  connected = disconnected = challengeOK = clockOK = false;
  setEpoch = 0;
  Serial.println("[B2] starting BLE only");
  BLEDevice::init(NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&serverEvents);
  BLEService* service = server->createService(SERVICE);
  auto* identity = service->createCharacteristic(IDENTITY, BLECharacteristic::PROPERTY_READ);
  char id[20] = {};
  snprintf(id, sizeof(id), "MMB2/" MOTO_MATE_VERSION "/%08lx", (unsigned long)esp_random());
  identity->setValue(std::string(id));
  auto* challenge = service->createCharacteristic(CHALLENGE,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  challenge->addDescriptor(new BLE2902());
  challenge->setCallbacks(&challengeEvents);
  auto* clock = service->createCharacteristic(CLOCK,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  clock->setCallbacks(&clockEvents);
  auto* metrics = service->createCharacteristic(METRICS, BLECharacteristic::PROPERTY_READ);
  uint8_t m[20] = {};
  writeLE(m, retained.run);
  writeLE(m+4, retained.wake);
  writeLE(m+8, retained.priorAwakeUs);
  writeLE(m+12, retained.priorFlags);
  writeLE(m+16, wakeCause);
  metrics->setValue(m, sizeof(m));
  service->start();
  advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.printf("[BLE] advertising %s identity=%s init_ms=%lu stop_boot_us=%lu\n",
      NAME, id, (unsigned long)(millis()-started), (unsigned long)STOP_US);
}

static bool pollRadio() {
  if (done) return true;
  if (!active) return false;
  if (disconnected.load() && uint32_t(millis()-discAt.load())>=250) {
    disconnected = false;
    if (completed.load()>=1) { finish(true); return true; }
    advertising->start();
    Serial.println("[BLE] advertising again");
  }
  if (uint64_t(esp_timer_get_time())>=STOP_US) finish(false);
  return done;
}

// USB-only: no external GPIO drive, no Wi-Fi/TFT/storage initialisation.
void sleepBenchBegin() {
  wakeCause = uint32_t(esp_sleep_get_wakeup_cause());
  if (wakeCause != ESP_SLEEP_WAKEUP_TIMER || retained.magic != MAGIC ||
      retained.wake == 0 || retained.wake >= MAX_WAKES) {
    retained = {MAGIC, esp_random(), 0, 0, 0};
  }
  ++retained.wake;
  Serial.begin(115200);
  Serial.printf("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " B2 run=%08lx wake=%lu cause=%lu prior_us=%lu prior_flags=%lu\n",
      (unsigned long)retained.run, (unsigned long)retained.wake,
      (unsigned long)wakeCause, (unsigned long)retained.priorAwakeUs,
      (unsigned long)retained.priorFlags);
  // Cold start after reset or actual deep sleep: radios should already be off.
  wifi_mode_t mode = WIFI_MODE_NULL;
  const esp_err_t w = esp_wifi_get_mode(&mode);
  if (!(w == ESP_ERR_WIFI_NOT_INIT || (w == ESP_OK && mode == WIFI_MODE_NULL))) {
    Serial.println("[B2] FAIL: Wi-Fi not off; radio test skipped");
    done = true;
    return;
  }
  beginRadio();
}

void sleepBenchPoll() {
  if (sleepArmed || !pollRadio()) return;
  sleepArmed = true;
  // Stop after three observed wakes; final sleep requires physical reset.
  // If Bluetooth could not shut down, deep sleep still cuts power to the radio,
  // but record shutdown failure rather than claiming a clean lifecycle.
  const bool terminal = retained.wake >= MAX_WAKES;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (!terminal && esp_sleep_enable_timer_wakeup(SLEEP_US) != ESP_OK) {
    Serial.println("[B2] FAIL: timer arm; reset required");
    return;
  }
  Serial.printf("[B2] sleep=%s local=%s off=%s pre_sleep_us=%llu\n",
      terminal ? "terminal/reset" : "300s", passed?"PASS":"FAIL",
      offVerified?"PASS":"FAIL", (unsigned long long)esp_timer_get_time());
  // No USB flush/wait: CDC can disappear in sleep; retained data is authoritative.
  retained.priorAwakeUs = uint32_t(esp_timer_get_time());
  retained.priorFlags = (passed ? 1U : 0U) | (offVerified ? 2U : 0U) |
      (retained.priorAwakeUs < TARGET_US ? 4U : 0U);
  esp_deep_sleep_start();
}
