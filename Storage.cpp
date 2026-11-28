#include "Storage.h"
#include "LogIdentity.h"
#include "LogSpool.h"
#include "LogMonitorPolicy.h"
#include "LogSleepCheckpoint.h"
#include "LogInventoryPolicy.h"
#include <esp_attr.h>
#include "BenchSensors.h"
#include "MotoClock.h"
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
#include <unistd.h>

namespace {
const char* fsState="not_mounted";
const char* logIdState="not_checked";
mm::log::Identity logId={};
bool attempted=false,mounted=false;
size_t partitionBytes=0,totalBytes=0,usedBytes=0;
uint64_t mountUs=0,statsUs=0,statsAt=0,lastLoop=0,maxLoopGap=0;
uint32_t sketchBytes=0,flashBytes=0;
struct MonitorSession {
  mm::log::Header header={};
  unsigned target=0,records=0;
  uint64_t due=0;
  bool active=false;
  const char* state="idle";
} monitor;
RTC_DATA_ATTR uint8_t sleepCheckpoint[mm::log::SLEEP_CHECKPOINT_BYTES]={};
constexpr unsigned AUTO_BATCH=96;
RTC_DATA_ATTR uint8_t autoCheckpoint[mm::log::SLEEP_CHECKPOINT_BYTES]={};
bool autoEnabled=false;
uint64_t autoDue=3000000ULL;
const char* autoState="disabled";
void forgetAuto() { memset(autoCheckpoint,0,sizeof(autoCheckpoint)); }
bool autoHeader(mm::log::Header& h,unsigned& count) {
  using namespace mm::log;
  uint8_t mac[6]={};
  return decodeSleepCheckpoint(autoCheckpoint,sizeof(autoCheckpoint),h,count,AUTO_BATCH) &&
    !strcmp(logIdState,"valid") && h.generation==logId.generation &&
    h.file_seq<=logId.reservedThrough && h.fw_major==1 && h.fw_minor==47 &&
    esp_efuse_mac_get_default(mac)==ESP_OK && !memcmp(mac,h.device_mac,6) &&
    verifySleepOpen(h,count);
}
bool sealAuto(const mm::log::Header& h,unsigned count) {
  using namespace mm::log;
  Scan scan={}; Suffix suffix=Suffix::Open;
  return recoverOpen(Kind::Monitor,h.generation,h.file_seq,scan,suffix) &&
    scan.classification==ScanClass::Complete && scan.records==count && suffix==Suffix::Bin;
}
void closeAutoForMaintenance() {
  if(!autoCheckpoint[0]) return;
  mm::log::Header h={}; unsigned count=0;
  const bool valid=autoHeader(h,count);
  forgetAuto(); // never reuse an uncertain checkpoint
  const bool ok=valid && sealAuto(h,count);
  autoState=ok?"sealed":"seal_failed_preserved";
  Serial.printf("[MONITOR] maintenance seal=%s records=%u\n",autoState,count);
}
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
  Serial.printf("[LOG] schema=1 identity=%s autonomous=%s\n",logIdState,autoEnabled?"enabled":"disabled");
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
  snprintf(b,sizeof(b),"{\"state\":\"%s\",\"device_mac\":%s,\"generation\":%s,\"reserved_through\":%s,\"next_reserved\":%s,\"filesystem_mounted\":%s,\"log_writes_enabled\":%s}",
    logIdState,macText,generation,high,nextText,mounted?"true":"false",autoEnabled && mounted && valid?"true":"false");
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
  h.kind=1; h.flags=1; h.fw_major=1; h.fw_minor=34;
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
const char* storageBenchAck(uint64_t generation,uint64_t seq,const String& supplied) {
  if(!mounted) return "unavailable";
  refreshIdentity();
  if(strcmp(logIdState,"valid") || logId.generation!=generation || !seq || seq>logId.reservedThrough) return "unavailable";
  fs::File file;
  String expected;
  if(!storageOpenBenchTransfer(generation,seq,file,expected)) {
    // No tombstone: absence is observable but is never proof of a prior ACK.
    return mm::log::benchIdentityAbsent(generation,seq)?"absent_unverified":"unavailable";
  }
  file.close();
  // Exact canonical descriptor from a fresh local read. The authenticated
  // BENCH host attests durability; firmware cannot verify host fsync itself.
  if(supplied!=expected) return "receipt_mismatch";
  char path[mm::log::PATH_BYTES],full[64];
  if(!mm::log::filename(mm::log::Kind::Monitor,generation,seq,mm::log::Suffix::Bin,path)) return "unavailable";
  const int n=snprintf(full,sizeof(full),"/littlefs%s",path);
  if(n<=0 || size_t(n)>=sizeof(full)) return "unavailable";
  // Single cooperative owner: no writer between fresh hash and unlink.
  const bool removed=unlink(full)==0;
  const bool absent=mm::log::benchIdentityAbsent(generation,seq);
  refreshSpace();
  if(!removed || !absent) return "removal_unconfirmed";
  Serial.printf("[LOG] BENCH durable ACK seq=%016llx removed; used=%u\\n",(unsigned long long)seq,unsigned(usedBytes));
  return "removed";
}
bool storageMonitorActive() { return monitor.active; }
String storageMonitorJson() {
  char b[300],seq[24]="null";
  if(monitor.header.file_seq) snprintf(seq,sizeof(seq),"\"%016llx\"",(unsigned long long)monitor.header.file_seq);
  snprintf(b,sizeof(b),"{\"state\":\"%s\",\"active\":%s,\"sequence\":%s,\"records\":%u,\"target\":%u,\"nominal_period_ms\":300000}",
    monitor.state,monitor.active?"true":"false",seq,monitor.records,monitor.target);
  return String(b);
}
bool storageMonitorStart(uint64_t generation,uint32_t bootId,unsigned count) {
  if(!mounted || monitor.active || !mm::log::benchCountOK(count)) return false;
  refreshIdentity(); refreshSpace();
  if(strcmp(logIdState,"valid") || generation!=logId.generation || usedBytes>totalBytes ||
     totalBytes-usedBytes<=16384+4096) return false;
  MonitorSession next;
  next.header.kind=1; next.header.flags=1; next.header.fw_major=1; next.header.fw_minor=34;
  if(esp_efuse_mac_get_default(next.header.device_mac)!=ESP_OK) return false;
  next.header.generation=generation; next.header.opened_boot_id=bootId; next.header.nominal_period_ms=300000;
  next.target=count; next.active=true; next.state="sampling";
  next.due=mm::log::nextMonitorDue(esp_timer_get_time(),true);
  monitor=next; return true;
}
void storageMonitorCancel(const char* reason) {
  if(!monitor.active) return;
  monitor.active=false; monitor.state=reason;
  Serial.printf("[LOG] BENCH monitor stopped: %s records=%u; preserve open file if present\n",reason,monitor.records);
}
bool storageMonitorSeal() {
  if(!monitor.active) return false;
  if(!monitor.records) { storageMonitorCancel("cancelled"); return true; }
  mm::log::Scan scan={}; mm::log::Suffix suffix=mm::log::Suffix::Open;
  const bool ok=mm::log::recoverOpen(mm::log::Kind::Monitor,monitor.header.generation,monitor.header.file_seq,scan,suffix) &&
    scan.classification==mm::log::ScanClass::Complete && scan.records==monitor.records;
  refreshSpace();
  monitor.active=false; monitor.state=ok?"complete":"seal_failed_preserved";
  return ok;
}
void storageMonitorPoll(uint64_t now) {
  if(!monitor.active || now<monitor.due) return;
  refreshSpace();
  if(usedBytes>totalBytes || totalBytes-usedBytes<=16384+4096) { storageMonitorCancel("storage_low_preserved"); return; }
  auto& h=monitor.header;
  mm::log::Sample sample={};
  mm::log::Prefix p={};
  p.boot_id=h.opened_boot_id;
  p.mono_ms=now/1000>=UINT32_MAX?UINT32_MAX:uint32_t(now/1000);
  p.type=1; p.state=0x46; // BENCH maintenance, no claim of riding or protection qualification
  p.utc_ms=clockLogUtcMs(p.validity);
  sensorsLogSample(now,sample,p.validity);
  // ADC rail estimate is deliberately NOT a qualified battery voltage. No GPS yet.
  uint8_t record[mm::log::RECORD_BYTES];
  if(!mm::log::encode(p,sample,record)) { storageMonitorCancel("encode_failed_preserved"); return; }
  if(!monitor.records) {
    if(!mm::log::allocateSequence(h.generation,h.file_seq)) { storageMonitorCancel("identity_failed"); return; }
    refreshIdentity();
    h.opened_utc_ms=p.utc_ms; h.opened_mono_ms=p.mono_ms;
    if(!mm::log::createOpen(h)) { storageMonitorCancel("create_failed_preserved"); return; }
  }
  if(!mm::log::appendOne(h,monitor.records,record)) { storageMonitorCancel("append_failed_preserved"); return; }
  ++monitor.records; refreshSpace();
  Serial.printf("[LOG] BENCH monitor seq=%016llx records=%u validity=%04x\n",
      (unsigned long long)h.file_seq,monitor.records,unsigned(p.validity));
  if(monitor.records>=monitor.target) { storageMonitorSeal(); return; }
  monitor.due=mm::log::nextMonitorDue(now,false);
}
bool storageSleepLogReady(uint64_t generation) {
  if(!mounted || monitor.active) return false;
  refreshIdentity(); refreshSpace();
  return !strcmp(logIdState,"valid") && generation==logId.generation &&
    usedBytes<=totalBytes && totalBytes-usedBytes>16384+4096;
}
void storageSleepForget() { memset(sleepCheckpoint,0,sizeof(sleepCheckpoint)); }
bool storageSleepSample(uint32_t bootId,uint32_t cycle) {
  using namespace mm::log;
  const uint64_t start=esp_timer_get_time();
  storageBeginMaintenance();
  Header h={}; unsigned count=0;
  bool resumed=false;
  if(cycle>1) resumed=decodeSleepCheckpoint(sleepCheckpoint,sizeof(sleepCheckpoint),h,count);
  storageSleepForget(); // consume before mutation; never resume uncertain writes
  bool ok=mounted && !strcmp(logIdState,"valid") && cycle>=1 && cycle<=SLEEP_LOG_CYCLES &&
    storageSleepLogReady(logId.generation);
  uint8_t mac[6]={};
  if(ok) ok=esp_efuse_mac_get_default(mac)==ESP_OK;
  if(ok && cycle>1) {
    ok=resumed && count==cycle-1 && h.generation==logId.generation &&
       h.file_seq<=logId.reservedThrough && h.fw_major==1 && h.fw_minor==34 &&
       !memcmp(mac,h.device_mac,6) && verifySleepOpen(h,count);
  }
  if(ok && cycle==1) {
    h.kind=1; h.flags=1; h.fw_major=1; h.fw_minor=34;
    memcpy(h.device_mac,mac,6); h.generation=logId.generation;
    h.nominal_period_ms=300000; h.opened_boot_id=bootId;
  }
  if(ok) {
    const uint64_t now=esp_timer_get_time();
    Prefix p={}; Sample sample={}; uint8_t record[RECORD_BYTES];
    p.boot_id=bootId; p.mono_ms=uint32_t(now/1000); p.type=1; p.state=0x46;
    p.utc_ms=clockLogUtcMs(p.validity); sensorsLogSample(now,sample,p.validity);
    ok=encode(p,sample,record);
    if(ok && !count) {
      h.opened_utc_ms=p.utc_ms; h.opened_mono_ms=p.mono_ms;
      ok=allocateSequence(h.generation,h.file_seq);
      refreshIdentity();
      if(ok) ok=benchIdentityAbsent(h.generation,h.file_seq) && createOpen(h);
    }
    if(ok) ok=appendOne(h,count,record);
    if(ok) ++count;
    if(ok && count==SLEEP_LOG_CYCLES) {
      Scan scan={}; Suffix suffix=Suffix::Open;
      ok=recoverOpen(Kind::Monitor,h.generation,h.file_seq,scan,suffix) &&
         scan.classification==ScanClass::Complete && scan.records==count;
    } else if(ok) ok=encodeSleepCheckpoint(h,count,sleepCheckpoint);
  }
  if(!ok) storageSleepForget();
  Serial.printf("[LOG] sleep cycle=%lu result=%s sequence=%016llx records=%u high=%016llx elapsed_us=%llu; sleep continues\n",
    (unsigned long)cycle,ok?(count==SLEEP_LOG_CYCLES?"complete":"committed_open"):"failed_preserved",
    (unsigned long long)h.file_seq,count,(unsigned long long)logId.reservedThrough,
    (unsigned long long)(esp_timer_get_time()-start));
  return ok;
}
void storageAutoBegin(bool enabled,bool retainedWake) {
  autoEnabled=enabled;
  autoState=enabled?"waiting":"disabled";
  autoDue=3000000ULL;
  if(!enabled || !retainedWake) forgetAuto();
}
void storageAutoPoll(uint64_t now,uint32_t bootId,bool maintenance) {
  using namespace mm::log;
  if(!autoEnabled || now<autoDue || monitor.active) return;
  // No catch-up bursts; one sample per ordinary BENCH wake, then 300s if awake.
  autoDue=now+MONITOR_PERIOD_US;
  const uint64_t start=esp_timer_get_time();
  Sample sample={}; Prefix p={};
  p.boot_id=bootId; p.mono_ms=uint32_t(now/1000); p.type=1;
  p.state=maintenance?0x46:0x41;
  p.utc_ms=clockLogUtcMs(p.validity);
  sensorsLogSample(now,sample,p.validity);
  // ADC remains an unqualified rail estimate, battery validity stays clear.
  storageBeginMaintenance();
  Header h={}; unsigned count=0;
  bool ok=mounted && storageSleepLogReady(logId.generation);
  const bool hadCheckpoint=autoCheckpoint[0]!=0;
  if(ok && hadCheckpoint) ok=autoHeader(h,count);
  forgetAuto(); // consume before mutation
  if(ok && !hadCheckpoint) {
    h.kind=1; h.flags=1; h.fw_major=1; h.fw_minor=47;
    h.generation=logId.generation; h.nominal_period_ms=300000;
    h.opened_boot_id=bootId; h.opened_utc_ms=p.utc_ms; h.opened_mono_ms=p.mono_ms;
    ok=esp_efuse_mac_get_default(h.device_mac)==ESP_OK;
  }
  uint8_t record[RECORD_BYTES];
  if(ok) ok=encode(p,sample,record);
  if(ok && !count) {
    ok=allocateSequence(h.generation,h.file_seq);
    refreshIdentity();
    if(ok) ok=benchIdentityAbsent(h.generation,h.file_seq) && createOpen(h);
  }
  if(ok) ok=appendOne(h,count,record);
  if(ok) ++count;
  const bool seal=maintenance || count>=AUTO_BATCH;
  if(ok) ok=seal?sealAuto(h,count):encodeSleepCheckpoint(h,count,autoCheckpoint,AUTO_BATCH);
  if(!ok) forgetAuto();
  autoState=ok?(seal?"sealed":"committed_open"):"failed_preserved";
  if(mounted) refreshSpace();
  Serial.printf("[MONITOR] %s seq=%016llx records=%u validity=%04x elapsed_us=%llu; sleep policy unchanged\n",
    autoState,(unsigned long long)h.file_seq,count,unsigned(p.validity),
    (unsigned long long)(esp_timer_get_time()-start));
}
void storageRecoverMaintenance() {
  using namespace mm::log;
  static bool done=false;
  if(done || !mounted || monitor.active) return;
  done=true;
  refreshIdentity();
  closeAutoForMaintenance();
  uint8_t mac[6]={};
  if(strcmp(logIdState,"valid") || esp_efuse_mac_get_default(mac)!=ESP_OK) return;
  DIR* d=opendir("/littlefs");
  if(!d) { Serial.println("[LOG] maintenance recovery directory unavailable"); return; }
  uint64_t pending[RECOVERY_MAX_FILES]={};
  unsigned entries=0,count=0,deferred=0;
  bool complete=true;
  while(true) {
    errno=0; dirent* e=readdir(d);
    if(!e) { if(errno) complete=false; break; }
    if(++entries>INVENTORY_MAX_ENTRIES) { complete=false; break; }
    uint64_t g=0,seq=0; Suffix suffix=Suffix::Bad;
    if(!parseMonitorName(e->d_name,g,seq,suffix) || g!=logId.generation ||
       seq>logId.reservedThrough || suffix!=Suffix::Open) continue;
    if(count<RECOVERY_MAX_FILES) pending[count++]=seq;
    else ++deferred;
  }
  if(closedir(d)!=0) complete=false;
  // No directory mutation during enumeration; refuse an incomplete scan.
  if(!complete) { Serial.println("[LOG] maintenance recovery scan incomplete; all originals retained"); return; }
  unsigned recovered=0,blocked=0;
  for(unsigned i=0;i<count;++i) {
    Scan scan={}; Suffix suffix=Suffix::Open;
    const bool ok=recoverBenchOpen(logId.generation,pending[i],mac,scan,suffix);
    if(ok) ++recovered; else ++blocked;
    Serial.printf("[LOG] auto recover seq=%016llx result=%s records=%u; original bytes retained\n",
      (unsigned long long)pending[i],ok?scanName(scan.classification):"blocked_preserved",unsigned(scan.records));
  }
  if(count) refreshSpace();
  Serial.printf("[LOG] maintenance recovery scanned=%u recovered=%u blocked=%u deferred=%u\n",entries,recovered,blocked,deferred);
}
bool storageInventory(String& result) {
  using namespace mm::log;
  if(!mounted || monitor.active) return false;
  refreshIdentity();
  uint8_t mac[6]={};
  if(strcmp(logIdState,"valid") || esp_efuse_mac_get_default(mac)!=ESP_OK) return false;
  String body;
  if(!body.reserve(6144)) return false;
  char b[256];
  snprintf(b,sizeof(b),"{\"device_mac\":\"%02x%02x%02x%02x%02x%02x\",\"generation\":\"%016llx\",\"reserved_through\":\"%016llx\",\"sealed_candidates\":[",
    unsigned(mac[0]),unsigned(mac[1]),unsigned(mac[2]),unsigned(mac[3]),unsigned(mac[4]),unsigned(mac[5]),
    (unsigned long long)logId.generation,(unsigned long long)logId.reservedThrough);
  if(!body.concat(b)) return false;
  DIR* d=opendir("/littlefs");
  if(!d) return false;
  unsigned entries=0,sealed=0,openCount=0,rec=0,bad=0,other=0;
  bool complete=true,ok=true;
  while(true) {
    errno=0; dirent* e=readdir(d);
    if(!e) { if(errno) complete=false; break; }
    if(++entries>INVENTORY_MAX_ENTRIES) { complete=false; break; }
    if(!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
    uint64_t g=0,seq=0; Suffix suffix=Suffix::Bad;
    if(!parseMonitorName(e->d_name,g,seq,suffix) || g!=logId.generation || seq>logId.reservedThrough) { ++other; continue; }
    switch(suffix) {
      case Suffix::Bin:
        snprintf(b,sizeof(b),"%s\"%016llx\"",sealed?",":"",(unsigned long long)seq);
        if(!body.concat(b)) ok=false;
        ++sealed; break;
      case Suffix::Open: ++openCount; break;
      case Suffix::Recovered: ++rec; break;
      case Suffix::Bad: ++bad; break;
      default: ++other; break;
    }
    if(!ok) break;
  }
  if(closedir(d)!=0) complete=false;
  if(!ok) return false;
  snprintf(b,sizeof(b),"],\"complete\":%s,\"open\":%u,\"recovered\":%u,\"bad\":%u,\"other\":%u}",
    complete?"true":"false",openCount,rec,bad,other);
  if(!body.concat(b)) return false;
  result=body;
  return result.length()==body.length();
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
  out+="\",\"log_writes_enabled\":"; out+=(autoEnabled && mounted && !strcmp(logIdState,"valid"))?"true":"false";
  out+=",\"auto_monitor\":{\"enabled\":"; out+=autoEnabled?"true":"false";
  out+=",\"state\":\""; out+=autoState; out+="\"},\"bench_monitor\":"; out+=storageMonitorJson(); out+="}";
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
