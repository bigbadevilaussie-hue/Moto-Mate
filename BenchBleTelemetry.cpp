#include "BenchBleTelemetry.h"
#include "Config.h"
#include "Hardware.h"
#include "BenchSensors.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_wifi.h>
#include <atomic>
#include <cmath>
#include <cstdio>

namespace {
constexpr char NAME[]="Moto-Mate";
constexpr char SERVICE[]="0da90001-7d91-4a75-9a7c-56d1f29b0731";
constexpr char IDENTITY[]="0da90002-7d91-4a75-9a7c-56d1f29b0731";
constexpr char TELEMETRY[]="0da90003-7d91-4a75-9a7c-56d1f29b0731";
BLEAdvertising* advertising=nullptr;
BLECharacteristic* telemetry=nullptr;
std::atomic<bool> disconnected{false};
std::atomic<uint32_t> disconnectedAt{0};
bool active=false;
uint64_t nextSample=0;
bool off() {
  return esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_IDLE &&
      esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_UNINITIALIZED;
}
class Events : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    disconnected=false;
    Serial.println("[BLE] BENCH connected");
  }
  void onDisconnect(BLEServer*) override {
    disconnectedAt=millis(); disconnected=true;
    Serial.println("[BLE] BENCH disconnected");
  }
};
Events events;
void put16(uint8_t* p,uint16_t v) {
  p[0]=uint8_t(v); p[1]=uint8_t(v>>8);
}
void put32(uint8_t* p,uint32_t v) {
  for(unsigned i=0;i<4;++i) p[i]=uint8_t(v>>(8*i));
}
}

bool benchBleBegin(uint32_t bootId) {
  wifi_mode_t mode=WIFI_MODE_NULL;
  const esp_err_t w=esp_wifi_get_mode(&mode);
  if (active || !off() || !(w==ESP_ERR_WIFI_NOT_INIT || (w==ESP_OK && mode==WIFI_MODE_NULL))) {
    Serial.println("[BLE] start refused: radio state");
    return false;
  }
  const uint32_t started=millis();
  // V1.08 B2 proven init/server/service/advertising ordering.
  active=true; disconnected=false; nextSample=0;
  BLEDevice::init(NAME);
  BLEServer* server=BLEDevice::createServer();
  server->setCallbacks(&events);
  BLEService* service=server->createService(SERVICE);
  auto* identity=service->createCharacteristic(IDENTITY,BLECharacteristic::PROPERTY_READ);
  char id[24]={};
  snprintf(id,sizeof(id),"MM/1/" MOTO_MATE_VERSION "/%08lx",(unsigned long)bootId);
  identity->setValue(std::string(id));
  telemetry=service->createCharacteristic(TELEMETRY,BLECharacteristic::PROPERTY_READ);
  uint8_t initial[20]={1};
  telemetry->setValue(initial,sizeof(initial));
  service->start();
  advertising=BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.printf("[BLE] advertising %s identity=%s init_ms=%lu\n",NAME,id,(unsigned long)(millis()-started));
  return true;
}
void benchBlePoll(uint64_t now,uint32_t cycle,uint32_t cause,uint64_t priorUs) {
  if (!active) return;
  if (disconnected.load() && uint32_t(millis()-disconnectedAt.load())>=250) {
    disconnected=false; advertising->start();
  }
  if (now<nextSample) return;
  nextSample=now+500000ULL;
  float temperature=NAN,humidity=NAN;
  sensorsBenchEnvironment(now,temperature,humidity);
  const float rail=hardwareBenchVoltage(now);
  uint8_t b[20]={1,0,uint8_t(cause),0};
  put32(b+4,cycle); put32(b+8,uint32_t(now/1000));
  if (std::isfinite(rail) && rail>=0 && rail<=65.535f) {
    b[1]|=1; put16(b+12,uint16_t(lroundf(rail*1000)));
  }
  if (std::isfinite(temperature) && temperature>=-40 && temperature<=125) {
    b[1]|=2; put16(b+14,uint16_t(int16_t(lroundf(temperature*100))));
  }
  if (std::isfinite(humidity) && humidity>=0 && humidity<=100) {
    b[1]|=4; put16(b+16,uint16_t(lroundf(humidity*100)));
  }
  const uint64_t priorSeconds=priorUs/1000000ULL;
  put16(b+18,uint16_t(priorSeconds>65535?65535:priorSeconds));
  telemetry->setValue(b,sizeof(b));
}
bool benchBleStop() {
  if (active) {
    active=false;
    if (advertising) advertising->stop();
    BLEDevice::deinit(false);
    advertising=nullptr; telemetry=nullptr;
    Serial.printf("[BLE] shutdown=%s\n",off()?"PASS":"FAIL");
  }
  return off();
}
