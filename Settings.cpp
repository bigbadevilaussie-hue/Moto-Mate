#include "Settings.h"
#include <nvs.h>
#include <esp_err.h>
#include <string.h>

namespace {
constexpr const char* NS="motomate";
constexpr const char* OLD_KEY="config_v1";
constexpr const char* KEY="config_v2";
mm::Settings current;
bool healthy=false;
const char* state="not_loaded";

esp_err_t readLegacy(uint32_t& word) {
  nvs_handle_t handle;
  esp_err_t err=nvs_open(NS,NVS_READONLY,&handle);
  if (err!=ESP_OK) return err;
  err=nvs_get_u32(handle,OLD_KEY,&word);
  nvs_close(handle);
  return err;
}
esp_err_t readRecord(uint8_t* bytes) {
  nvs_handle_t handle;
  esp_err_t err=nvs_open(NS,NVS_READONLY,&handle);
  if (err!=ESP_OK) return err;
  size_t n=mm::SETTINGS_BYTES;
  err=nvs_get_blob(handle,KEY,bytes,&n);
  nvs_close(handle);
  if (err==ESP_OK && n!=mm::SETTINGS_BYTES) return ESP_ERR_NVS_INVALID_LENGTH;
  return err;
}
bool writeRecord(const mm::Settings& next) {
  uint8_t bytes[mm::SETTINGS_BYTES],check[mm::SETTINGS_BYTES];
  if (!mm::encodeV2(next,bytes,sizeof(bytes))) return false;
  nvs_handle_t handle;
  esp_err_t err=nvs_open(NS,NVS_READWRITE,&handle);
  if (err!=ESP_OK) return false;
  err=nvs_set_blob(handle,KEY,bytes,sizeof(bytes));
  if (err==ESP_OK) err=nvs_commit(handle);
  nvs_close(handle);
  if (err!=ESP_OK) return false;
  mm::Settings verified;
  return readRecord(check)==ESP_OK && memcmp(bytes,check,sizeof(bytes))==0 &&
      mm::decodeV2(check,sizeof(check),verified);
}
}

void settingsBegin() {
  current=mm::Settings{};
  healthy=false;
  uint8_t bytes[mm::SETTINGS_BYTES];
  const esp_err_t err=readRecord(bytes);
  if (err==ESP_OK) {
    healthy=mm::decodeV2(bytes,sizeof(bytes),current);
    state=healthy?"loaded":"invalid_schema";
  } else if (err==ESP_ERR_NVS_NOT_FOUND) {
    uint32_t old=0;
    const esp_err_t legacy=readLegacy(old);
    if (legacy==ESP_OK && !mm::decode(old,current)) {
      state="invalid_legacy";
    } else if (legacy==ESP_OK || legacy==ESP_ERR_NVS_NOT_FOUND) {
      healthy=writeRecord(current);
      state=healthy?(legacy==ESP_OK?"migrated":"initialised"):"write_failed";
    } else {
      state="legacy_read_failed";
    }
  } else {
    // An existing unreadable v2 record must NEVER fall back to stale v1.
    state="read_failed";
  }
  Serial.printf("[NVS] settings=%s schema=2 requested=%s\n",state,
      current.profile==mm::Profile::Bench?"BENCH":"FIELD");
}
const mm::Settings& settingsCurrent() { return current; }
bool settingsHealthy() { return healthy; }

bool settingsSave(const mm::Settings& next) {
  if (!healthy || !mm::settingsValid(next)) return false;
  uint8_t oldBytes[mm::SETTINGS_BYTES],newBytes[mm::SETTINGS_BYTES];
  if (!mm::encodeV2(current,oldBytes,sizeof(oldBytes)) ||
      !mm::encodeV2(next,newBytes,sizeof(newBytes))) return false;
  if (memcmp(oldBytes,newBytes,sizeof(oldBytes))==0) return true;
  if (!writeRecord(next)) {
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
  char out[480];
  snprintf(out,sizeof(out),
    "{\"profile\":\"%s\",\"effective_profile\":\"%s\",\"bench_sleep\":%s,\"bench_sleep_active\":%s,\"bench_sleep_log\":%s,\"verbose\":%s,\"verbose_active\":%s,\"field_ready\":%s,\"bench_sleep_ready\":%s,\"nvs_ok\":%s,\"nvs_state\":\"%s\",\"schema\":1,\"storage_schema\":2,\"config_active\":false",
    current.profile==mm::Profile::Bench?"BENCH":"FIELD",
    recovery?"RECOVERY":(field?"FIELD":"BENCH"),
    current.benchSleep?"true":"false",
    healthy && mm::benchSleepActive(current)?"true":"false",
    current.benchSleepLog?"true":"false",
    current.verbose?"true":"false",
    healthy && mm::verboseActive(current)?"true":"false",
    mm::FIELD_READY?"true":"false",mm::BENCH_SLEEP_READY?"true":"false",
    healthy?"true":"false",state);
  String body(out);
  body.reserve(1800);
  body += ",\"config\":{";
  bool first=true;
#define MM_JSON(name, def, lo, hi) \
  if (!first) body += ','; \
  first=false; body += "\"" #name "\":"; body += long(current.name);
  MM_SETTINGS_FIELDS(MM_JSON)
#undef MM_JSON
  body += "}}";
  return body;
}
