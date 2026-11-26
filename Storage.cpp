#include "Storage.h"
#include "LogIdentity.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <nvs.h>

namespace {
const char* fsState="not_mounted";
const char* logIdState="not_checked";
bool attempted=false,mounted=false;
size_t partitionBytes=0,totalBytes=0,usedBytes=0;
uint64_t mountUs=0,statsUs=0,statsAt=0,lastLoop=0,maxLoopGap=0;
uint32_t sketchBytes=0,flashBytes=0;
}
void storageBeginMaintenance() {
  if (attempted) return;
  attempted=true;
  mm::log::Identity id={};
  logIdState=mm::log::identityStateName(mm::log::readIdentity(id));
  Serial.printf("[LOG] schema=1 identity=%s writes=disabled\n",logIdState);
  const auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  partitionBytes=p?p->size:0;
  flashBytes=ESP.getFlashChipSize();
  sketchBytes=ESP.getSketchSize();
  const uint64_t start=esp_timer_get_time();
  if (!p) fsState="partition_missing";
  else if (!LittleFS.begin(false)) fsState="mount_failed_no_format";
  else { mounted=true; fsState="mounted"; }
  mountUs=uint64_t(esp_timer_get_time())-start;
  if (mounted) {
    const uint64_t s=esp_timer_get_time();
    totalBytes=LittleFS.totalBytes();
    usedBytes=LittleFS.usedBytes();
    statsAt=esp_timer_get_time();
    statsUs=statsAt-s;
  }
  Serial.printf("[STORAGE] LittleFS=%s partition=%lu total=%lu used=%lu mount_us=%llu stats_us=%llu; NO AUTO FORMAT\n",
    fsState,(unsigned long)partitionBytes,(unsigned long)totalBytes,(unsigned long)usedBytes,
    (unsigned long long)mountUs,(unsigned long long)statsUs);
  Serial.printf("[MEMORY] flash=%lu image=%lu heap_free=%lu heap_min=%lu\n",
    (unsigned long)flashBytes,(unsigned long)sketchBytes,(unsigned long)ESP.getFreeHeap(),
    (unsigned long)ESP.getMinFreeHeap());
}
String storageJson() {
  char b[380];
  if (!mounted) {
    snprintf(b,sizeof(b),"{\"state\":\"%s\",\"partition_bytes\":%lu,\"total_bytes\":null,\"used_bytes\":null,\"free_bytes\":null,\"mount_us\":%llu,\"stats_us\":null,\"stats_age_ms\":null}",
      fsState,(unsigned long)partitionBytes,(unsigned long long)mountUs);
  } else {
    const size_t free=usedBytes<=totalBytes?totalBytes-usedBytes:0;
    snprintf(b,sizeof(b),"{\"state\":\"%s\",\"partition_bytes\":%lu,\"total_bytes\":%lu,\"used_bytes\":%lu,\"free_bytes\":%lu,\"mount_us\":%llu,\"stats_us\":%llu,\"stats_age_ms\":%llu}",
      fsState,(unsigned long)partitionBytes,(unsigned long)totalBytes,(unsigned long)usedBytes,
      (unsigned long)free,(unsigned long long)mountUs,(unsigned long long)statsUs,
      (unsigned long long)((uint64_t(esp_timer_get_time())-statsAt)/1000));
  }
  String out(b);
  out.remove(out.length()-1);
  out+=",\"log_schema\":1,\"log_identity\":\""; out+=logIdState;
  out+="\",\"log_writes_enabled\":false}";
  return out;
}
void resourcesObserveLoop(uint64_t now) {
  if (lastLoop && now>=lastLoop && now-lastLoop>maxLoopGap) maxLoopGap=now-lastLoop;
  lastLoop=now;
}
String resourcesJson() {
  multi_heap_info_t heap={};
  heap_caps_get_info(&heap,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  nvs_stats_t nvs={};
  const bool nvsOK=nvs_get_stats(nullptr,&nvs)==ESP_OK;
  char b[400];
  snprintf(b,sizeof(b),"{\"flash_bytes\":%lu,\"image_bytes\":%lu,\"internal_heap_free_bytes\":%lu,\"internal_heap_min_free_bytes\":%lu,\"internal_heap_largest_block_bytes\":%lu,\"max_loop_gap_us\":%llu,\"nvs\":",
    (unsigned long)flashBytes,(unsigned long)sketchBytes,(unsigned long)heap.total_free_bytes,
    (unsigned long)heap.minimum_free_bytes,(unsigned long)heap.largest_free_block,(unsigned long long)maxLoopGap);
  String out(b);
  if (nvsOK) {
    snprintf(b,sizeof(b),"{\"used_entries\":%lu,\"free_entries\":%lu,\"total_entries\":%lu,\"namespaces\":%lu}",
      (unsigned long)nvs.used_entries,(unsigned long)nvs.free_entries,(unsigned long)nvs.total_entries,
      (unsigned long)nvs.namespace_count);
    out+=b;
  } else out+="null";
  out+="}";
  return out;
}
