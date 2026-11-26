#include "BenchBle.h"
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
constexpr char NAME[] = "Moto-Mate-B1";
constexpr char SERVICE[] = "0da70001-7d91-4a75-9a7c-56d1f29b0731";
constexpr char IDENTITY[] = "0da70002-7d91-4a75-9a7c-56d1f29b0731";
constexpr char CHALLENGE[] = "0da70003-7d91-4a75-9a7c-56d1f29b0731";
constexpr char CLOCK[] = "0da70004-7d91-4a75-9a7c-56d1f29b0731";
constexpr uint32_t LIMIT_MS = 40000;
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
  passed = handshake && off;
  done = true;
  Serial.printf("[BLE] local sessions=%u elapsed_ms=%lu shutdown=%s result=%s\n",
      completed.load(), (unsigned long)(millis()-started),
      off?"PASS":"FAIL", passed?"PASS":"FAIL");
  Serial.println("[OSA] verify host challenge/clock results independently");
}
}

void commissionBleBegin() {
  started = millis();
  active = true;
  done = passed = false;
  completed = 0;
  connected = disconnected = challengeOK = clockOK = false;
  setEpoch = 0;
  Serial.println("\n--- 06 / BLE & OSA B1 ---------------------------------");
  BLEDevice::init(NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&serverEvents);
  BLEService* service = server->createService(SERVICE);
  auto* identity = service->createCharacteristic(IDENTITY, BLECharacteristic::PROPERTY_READ);
  char id[20] = {};
  snprintf(id, sizeof(id), "MMB1/" MOTO_MATE_VERSION "/%08lx", (unsigned long)esp_random());
  identity->setValue(std::string(id));
  auto* challenge = service->createCharacteristic(CHALLENGE,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  challenge->addDescriptor(new BLE2902());
  challenge->setCallbacks(&challengeEvents);
  auto* clock = service->createCharacteristic(CLOCK,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  clock->setCallbacks(&clockEvents);
  service->start();
  advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.printf("[BLE] advertising %s identity=%s init_ms=%lu window=%lu ms\n",
      NAME, id, (unsigned long)(millis()-started), (unsigned long)LIMIT_MS);
}

bool commissionBlePoll() {
  if (done) return true;
  if (!active) return false;
  if (disconnected.load() && uint32_t(millis()-discAt.load())>=250) {
    disconnected = false;
    if (completed.load()>=2) { finish(true); return true; }
    advertising->start();
    Serial.println("[BLE] advertising again");
  }
  if (uint32_t(millis()-started)>=LIMIT_MS) finish(false);
  return done;
}
bool commissionBlePassed() { return passed; }
void commissionBleCancel() { if (active) finish(false); }
