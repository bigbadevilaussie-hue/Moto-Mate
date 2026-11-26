#include "MotoClock.h"
#include "ClockPolicy.h"
#include "Rtc8563.h"
#include <esp_attr.h>
#include <esp_timer.h>
#include <nvs.h>
#include <sys/time.h>
#include <time.h>

namespace {
constexpr uint32_t CLOCK_MAGIC=0x4d4d0121;
RTC_DATA_ATTR uint32_t retainedMagic=0;
RTC_DATA_ATTR uint64_t retainedSyncUs=0;
enum class Quality { Unknown, Synced, Holdover };
Quality quality=Quality::Unknown;
uint64_t checkpointUs=0;
const char* checkpointState="missing";
bool correctionKnown=false;
int64_t lastCorrection=0;

uint64_t platformUtcUs() {
  timeval tv={};
  if (gettimeofday(&tv,nullptr)!=0 || tv.tv_sec<0 || tv.tv_usec<0 || tv.tv_usec>=1000000)
    return 0;
  const uint64_t us=uint64_t(tv.tv_sec)*1000000ULL+uint64_t(tv.tv_usec);
  return mm::utcInRange(us)?us:0;
}
const char* qualityName(uint64_t raw) {
  if (!raw || quality==Quality::Unknown) return "unknown";
  return quality==Quality::Synced?"host_synced":"rtc_holdover";
}
void readCheckpoint() {
  nvs_handle_t h;
  esp_err_t e=nvs_open("clock",NVS_READONLY,&h);
  if (e==ESP_ERR_NVS_NOT_FOUND) { checkpointState="missing"; return; }
  if (e!=ESP_OK) { checkpointState="read_failed"; return; }
  uint32_t s=0;
  e=nvs_get_u32(h,"epoch",&s);
  nvs_close(h);
  if (e==ESP_ERR_NVS_NOT_FOUND) { checkpointState="missing"; return; }
  if (e!=ESP_OK) { checkpointState="read_failed"; return; }
  const uint64_t us=uint64_t(s)*1000000ULL;
  if (!mm::utcInRange(us)) { checkpointState="invalid"; return; }
  checkpointUs=us;
  checkpointState="loaded_reference";
}
void checkpoint(uint64_t us) {
  if (!mm::checkpointDue(checkpointUs,us)) return;
  // Keep the donor NVS key/uint32 seconds format. Never auto-erase NVS.
  nvs_handle_t h;
  esp_err_t e=nvs_open("clock",NVS_READWRITE,&h);
  if (e!=ESP_OK) { checkpointState="write_failed"; return; }
  const uint32_t s=uint32_t(us/1000000ULL);
  e=nvs_set_u32(h,"epoch",s);
  if (e==ESP_OK) e=nvs_commit(h);
  uint32_t check=0;
  if (e==ESP_OK) e=nvs_get_u32(h,"epoch",&check);
  nvs_close(h);
  if (e!=ESP_OK || check!=s) { checkpointState="write_failed"; return; }
  checkpointUs=uint64_t(s)*1000000ULL;
  checkpointState="saved_reference";
}
String numberOrNull(uint64_t n) {
  if (!n) return String("null");
  char b[24];
  snprintf(b,sizeof(b),"%llu",(unsigned long long)n);
  return String(b);
}
}

void clockBegin(bool deepSleepWake) {
  readCheckpoint();
  const uint64_t raw=platformUtcUs();
  if (deepSleepWake && retainedMagic==CLOCK_MAGIC && mm::utcInRange(retainedSyncUs)
      && raw>=retainedSyncUs) {
    quality=Quality::Holdover;
  } else {
    retainedMagic=0;
    retainedSyncUs=0;
    quality=Quality::Unknown;
  }
  Serial.printf("[CLOCK] quality=%s platform_utc_us=%llu checkpoint=%s saved_us=%llu\n",
    qualityName(raw),(unsigned long long)raw,checkpointState,(unsigned long long)checkpointUs);
}

uint64_t clockUtcUs() {
  return quality==Quality::Unknown?0:platformUtcUs();
}

bool clockApplyHostUtc(uint64_t us) {
  if (!mm::utcInRange(us)) return false;
  const uint64_t before=platformUtcUs();
  timeval tv={};
  tv.tv_sec=time_t(us/1000000ULL);
  tv.tv_usec=long(us%1000000ULL);
  if (settimeofday(&tv,nullptr)!=0) return false;
  const uint64_t after=platformUtcUs();
  if (!after || after<us || after-us>1000000ULL) {
    quality=Quality::Unknown;
    retainedMagic=0;
    retainedSyncUs=0;
    return false;
  }
  correctionKnown=before!=0;
  lastCorrection=correctionKnown?mm::correctionUs(before,us):0;
  quality=Quality::Synced;
  retainedSyncUs=us;
  retainedMagic=CLOCK_MAGIC;
  checkpoint(us);
  rtc8563Sync(clockUtcUs());
  Serial.printf("[CLOCK] host sync utc_us=%llu correction_known=%d correction_us=%lld checkpoint=%s\n",
    (unsigned long long)us,int(correctionKnown),(long long)lastCorrection,checkpointState);
  return true;
}

String clockJson() {
  const uint64_t raw=platformUtcUs();
  const uint64_t utc=quality==Quality::Unknown?0:raw;
  char b[96];
  String out="{\"quality\":\"";
  out+=qualityName(raw);
  out+="\",\"utc_us\":";
  out+=numberOrNull(utc);
  out+=",\"platform_utc_us\":";
  out+=numberOrNull(raw);
  out+=",\"last_sync_us\":";
  out+=numberOrNull(retainedSyncUs);
  out+=",\"checkpoint_us\":";
  out+=numberOrNull(checkpointUs);
  out+=",\"checkpoint_state\":\"";
  out+=checkpointState;
  out+="\",\"correction_us\":";
  if (correctionKnown) {
    snprintf(b,sizeof(b),"%lld",(long long)lastCorrection);
    out+=b;
  } else out+="null";
  snprintf(b,sizeof(b),",\"monotonic_us\":%llu",(unsigned long long)esp_timer_get_time());
  out+=b;
  out+=",\"external_rtc\":";
  out+=rtc8563Json(clockUtcUs());
  out+="}";
  return out;
}
