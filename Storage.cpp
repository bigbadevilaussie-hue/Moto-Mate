#include "Storage.h"
#include "LogIdentity.h"
#include "LogSpool.h"
#include "Config.h"
#include <esp_sleep.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <nvs.h>
#include <esp_system.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <mbedtls/sha256.h>

namespace {
const char* fsState="not_mounted";
const char* logIdState="not_checked";
mm::log::Identity logId={};
bool attempted=false,mounted=false;
size_t partitionBytes=0,totalBytes=0,usedBytes=0;
uint64_t mountUs=0,statsUs=0,statsAt=0,lastLoop=0,maxLoopGap=0;
uint32_t sketchBytes=0,flashBytes=0;
void refreshSpace() {
  if(!mounted) return;
  const uint64_t start=esp_timer_get_time();
  totalBytes=LittleFS.totalBytes(); usedBytes=LittleFS.usedBytes();
  statsAt=esp_timer_get_time(); statsUs=statsAt-start;
}
void refreshIdentity() {
  logId={};
  logIdState=mm::log::identityStateName(mm::log::readIdentity(logId));
}
bool virginFilesystem() {
  if(!mounted) return false;
  // LittleFS.begin default VFS mount path on Arduino ESP32 2.0.17.
  // Fail closed on any existing entry (including unknown files) or read error.
  DIR* d=opendir("/littlefs");
  if(!d) return false;
  bool empty=true;
  while(true) {
    errno=0; dirent* e=readdir(d);
    if(!e) { if(errno) empty=false; break; }
    if(strcmp(e->d_name,".") && strcmp(e->d_name,"..")) { empty=false; break; }
  }
  return closedir(d)==0 && empty;
}
}
void storageBeginMaintenance() {
  if (attempted) return;
  attempted=true;
  refreshIdentity();
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
String storageLogIdentityJson() {
  uint8_t mac[6]={};
  const bool macOK=esp_efuse_mac_get_default(mac)==ESP_OK;
  uint64_t next=0;
  const bool valid=!strcmp(logIdState,"valid");
  const bool cursor=valid && mm::log::currentCursor(logId.generation,logId.reservedThrough,next);
  char b[360];
  char generation[20]="null",high[20]="null",nextText[20]="null",macText[16]="null";
  if(valid) {
    snprintf(generation,sizeof(generation),"\"%016llx\"",(unsigned long long)logId.generation);
    snprintf(high,sizeof(high),"\"%016llx\"",(unsigned long long)logId.reservedThrough);
  }
  if(cursor) snprintf(nextText,sizeof(nextText),"\"%016llx\"",(unsigned long long)next);
  if(macOK) snprintf(macText,sizeof(macText),"\"%02x%02x%02x%02x%02x%02x\"",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  snprintf(b,sizeof(b),"{\"state\":\"%s\",\"device_mac\":%s,\"generation\":%s,\"reserved_through\":%s,\"next_reserved\":%s,\"filesystem_mounted\":%s,\"log_writes_enabled\":false}",
    logIdState,macText,generation,high,nextText,mounted?"true":"false");
  return String(b);
}
bool storageProvisionLogIdentity(uint64_t generation) {
  if(!virginFilesystem()) return false;
  const bool ok=mm::log::provisionVirginIdentity(generation);
  refreshIdentity();
  return ok;
}
bool storageAllocateTestSequence(uint64_t generation,uint64_t& seq) {
  if(!mounted) return false;
  const bool ok=mm::log::allocateSequence(generation,seq);
  refreshIdentity();
  return ok;
}
bool storageTestLog(uint64_t generation,uint32_t bootId,bool leaveOpen,String& result) {
  if(!mounted) return false;
  refreshIdentity(); refreshSpace();
  if(strcmp(logIdState,"valid") || logId.generation!=generation ||
     usedBytes>totalBytes || totalBytes-usedBytes<=16384+4096) return false;
  mm::log::Header h={};
  h.kind=1; h.flags=1; h.fw_major=1; h.fw_minor=28;
  if(esp_efuse_mac_get_default(h.device_mac)!=ESP_OK) return false;
  h.generation=generation; h.nominal_period_ms=300000; h.opened_boot_id=bootId;
  const uint64_t ms=uint64_t(esp_timer_get_time())/1000;
  h.opened_mono_ms=ms>UINT32_MAX?UINT32_MAX:uint32_t(ms);
  // Real observed boot/wake evidence, not fabricated voltage or time.
  mm::log::Prefix p={0,bootId,h.opened_mono_ms,7,0x46,0};
  mm::log::Boot boot={uint16_t(esp_reset_reason()),uint16_t(esp_sleep_get_wakeup_cause()),UINT32_MAX,UINT32_MAX,0,0};
  uint8_t record[mm::log::RECORD_BYTES];
  if(!mm::log::encode(p,boot,record) || !mm::log::allocateSequence(generation,h.file_seq)) return false;
  refreshIdentity();
  const bool written=mm::log::createOpen(h) && mm::log::appendOne(h,0,record);
  mm::log::Scan scan={}; mm::log::Suffix suffix=mm::log::Suffix::Open;
  const bool ok=written && (leaveOpen || mm::log::recoverOpen(mm::log::Kind::Monitor,generation,h.file_seq,scan,suffix));
  refreshSpace();
  char path[mm::log::PATH_BYTES],b[240];
  if(!mm::log::filename(mm::log::Kind::Monitor,generation,h.file_seq,suffix,path)) return false;
  snprintf(b,sizeof(b),"{\"ok\":%s,\"sequence\":\"%016llx\",\"path\":\"%s\",\"bytes\":%u,\"records\":%u,\"state\":\"%s\"}",
    ok?"true":"false",(unsigned long long)h.file_seq,path,
    unsigned(ok&&leaveOpen?128:scan.bytes),unsigned(ok&&leaveOpen?1:scan.records),
    ok?(leaveOpen?"open":mm::log::scanName(scan.classification)):"failed_preserved");
  result=b; return ok && (leaveOpen || scan.classification==mm::log::ScanClass::Complete);
}
bool storageRecoverTestLog(uint64_t generation,uint64_t seq,String& result) {
  if(!mounted) return false;
  refreshIdentity();
  if(strcmp(logIdState,"valid") || generation!=logId.generation || !seq || seq>logId.reservedThrough) return false;
  mm::log::Scan scan={}; mm::log::Suffix suffix=mm::log::Suffix::Open;
  const bool ok=mm::log::recoverOpen(mm::log::Kind::Monitor,generation,seq,scan,suffix);
  refreshSpace();
  char path[mm::log::PATH_BYTES],b[240];
  if(!mm::log::filename(mm::log::Kind::Monitor,generation,seq,suffix,path)) return false;
  snprintf(b,sizeof(b),"{\"ok\":%s,\"sequence\":\"%016llx\",\"path\":\"%s\",\"bytes\":%u,\"records\":%u,\"state\":\"%s\"}",
    ok?"true":"false",(unsigned long long)seq,path,unsigned(scan.bytes),unsigned(scan.records),
    ok?mm::log::scanName(scan.classification):"failed_preserved");
  result=b; return ok;
}
bool storageOpenBenchTransfer(uint64_t generation,uint64_t seq,fs::File& file,String& descriptor) {
  if(!mounted || file) return false;
  refreshIdentity();
  if(strcmp(logIdState,"valid") || logId.generation!=generation || !seq || seq>logId.reservedThrough) return false;
  mm::log::Scan scan={};
  if(!mm::log::inspectSealedMonitor(generation,seq,scan)) return false;
  char path[mm::log::PATH_BYTES];
  if(!mm::log::filename(mm::log::Kind::Monitor,generation,seq,mm::log::Suffix::Bin,path)) return false;
  fs::File f=LittleFS.open(path,"r");
  if(!f || f.size()!=scan.bytes) return false;
  uint8_t header[mm::log::HEADER_BYTES],mac[6]={};
  if(f.read(header,sizeof(header))!=sizeof(header) ||
     !mm::log::validHeader(header,sizeof(header)) ||
     !mm::log::filenameMatchesHeader(path,header,sizeof(header)) ||
     !(header[10]&1u) || esp_efuse_mac_get_default(mac)!=ESP_OK ||
     memcmp(mac,header+12,6) || !f.seek(0)) {
    f.close(); return false;
  }
  uint8_t chunk[512],digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  bool ok=mbedtls_sha256_starts_ret(&ctx,0)==0;
  size_t remaining=scan.bytes;
  while(ok && remaining) {
    const size_t n=remaining<sizeof(chunk)?remaining:sizeof(chunk);
    ok=f.read(chunk,n)==n && mbedtls_sha256_update_ret(&ctx,chunk,n)==0;
    remaining-=n;
  }
  ok=ok && mbedtls_sha256_finish_ret(&ctx,digest)==0;
  mbedtls_sha256_free(&ctx);
  if(!ok || !f.seek(0)) { f.close(); return false; }
  char hash[65];
  for(unsigned i=0;i<sizeof(digest);++i) snprintf(hash+i*2,3,"%02x",unsigned(digest[i]));
  char b[512];
  const int n=snprintf(b,sizeof(b),"{\"device_mac\":\"%02x%02x%02x%02x%02x%02x\",\"file_id\":\"%016llx%016llx\",\"basename\":\"%s\",\"encoding\":\"raw\",\"transmitted_bytes\":%u,\"transmitted_sha256\":\"%s\",\"canonical_raw_bytes\":%u,\"canonical_raw_sha256\":\"%s\"}",
    unsigned(mac[0]),unsigned(mac[1]),unsigned(mac[2]),unsigned(mac[3]),unsigned(mac[4]),unsigned(mac[5]),
    (unsigned long long)generation,(unsigned long long)seq,path+1,unsigned(scan.bytes),hash,unsigned(scan.bytes),hash);
  if(n<=0 || size_t(n)>=sizeof(b)) { f.close(); return false; }
  descriptor=b; file=f;
  return bool(file);
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
