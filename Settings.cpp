#include "Settings.h"
#include <nvs.h>
#include <esp_err.h>

namespace {
constexpr const char* NS="motomate";
constexpr const char* KEY="config_v1";
mm::Settings current;
bool healthy=false;
const char* state="not_loaded";

bool readWord(uint32_t& word, esp_err_t& err) {
  nvs_handle_t handle;
  err=nvs_open(NS,NVS_READONLY,&handle);
  if (err!=ESP_OK) return false;
  err=nvs_get_u32(handle,KEY,&word);
  nvs_close(handle);
  return err==ESP_OK;
}
bool writeWord(uint32_t word) {
  nvs_handle_t handle;
  esp_err_t err=nvs_open(NS,NVS_READWRITE,&handle);
  if (err!=ESP_OK) return false;
  err=nvs_set_u32(handle,KEY,word);
  if (err==ESP_OK) err=nvs_commit(handle);
  nvs_close(handle);
  if (err!=ESP_OK) return false;
  uint32_t check=0;
  return readWord(check,err) && check==word;
}
}

void settingsBegin() {
  current=mm::Settings{};
  uint32_t word=0;
  esp_err_t err=ESP_OK;
  if (readWord(word,err)) {
    healthy=mm::decode(word,current);
    state=healthy?"loaded":"invalid_schema";
  } else if (err==ESP_ERR_NVS_NOT_FOUND) {
    healthy=writeWord(mm::encode(current));
    state=healthy?"initialised":"write_failed";
  } else {
    healthy=false;
    state="read_failed";
  }
  Serial.printf("[NVS] settings=%s requested=%s\n",state,
      current.profile==mm::Profile::Bench?"BENCH":"FIELD");
}
const mm::Settings& settingsCurrent() { return current; }
bool settingsHealthy() { return healthy; }

bool settingsSave(const mm::Settings& next) {
  if (!healthy) return false; // Never silently overwrite corrupt/unknown storage.
  if (mm::encode(next)==mm::encode(current)) return true; // no flash wear
  if (!writeWord(mm::encode(next))) {
    healthy=false;
    state="write_failed";
    return false;
  }
  current=next;
  state="saved";
  return true;
}
String settingsJson() {
  const bool field=healthy && mm::fieldActive(current);
  const bool recovery=!healthy || (current.profile==mm::Profile::Field && !field);
  char out[420];
  snprintf(out,sizeof(out),
    "{\"profile\":\"%s\",\"effective_profile\":\"%s\",\"bench_sleep\":%s,\"bench_sleep_active\":%s,\"verbose\":%s,\"verbose_active\":%s,\"field_ready\":%s,\"bench_sleep_ready\":%s,\"nvs_ok\":%s,\"nvs_state\":\"%s\",\"schema\":1}",
    current.profile==mm::Profile::Bench?"BENCH":"FIELD",
    recovery?"RECOVERY":(field?"FIELD":"BENCH"),
    current.benchSleep?"true":"false",
    healthy && mm::benchSleepActive(current)?"true":"false",
    current.verbose?"true":"false",
    healthy && mm::verboseActive(current)?"true":"false",
    mm::FIELD_READY?"true":"false",mm::BENCH_SLEEP_READY?"true":"false",
    healthy?"true":"false",state);
  return String(out);
}
