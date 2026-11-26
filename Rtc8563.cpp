#include "Rtc8563.h"
#include "RtcPolicy.h"
#include "Config.h"
#include <Wire.h>
#include <esp_timer.h>

namespace {
bool busReady=false;
bool present=false;
bool lowVoltage=false;
bool stopped=false;
uint64_t rtcUtc=0;
const char* state="disabled";
const char* writeState="not_requested";
constexpr uint8_t ADDRESS=RTC_PCF8563_ADDRESS;
bool readRegs(uint8_t reg,uint8_t* data,size_t n) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false)!=0) return false;
  if (Wire.requestFrom(ADDRESS,n,true)!=n) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i=0;i<n;++i) {
    const int v=Wire.read();
    if (v<0) return false;
    data[i]=uint8_t(v);
  }
  return true;
}
bool writeRegs(uint8_t reg,const uint8_t* data,size_t n) {
  Wire.beginTransmission(ADDRESS);
  const bool queued=Wire.write(reg)==1 && Wire.write(data,n)==n;
  const uint8_t result=Wire.endTransmission(true);
  return queued && result==0;
}
bool sample() {
  rtcUtc=0; lowVoltage=false; stopped=false; present=false;
  if (!busReady) return false;
  Wire.beginTransmission(ADDRESS);
  const uint8_t result=Wire.endTransmission(true);
  if (result!=0) { state=result==2?"absent":"probe_error"; return false; }
  present=true;
  uint8_t b[9]={};
  // One sequential register read avoids inconsistent rollover snapshots.
  if (!readRegs(0,b,sizeof(b))) { state="read_error"; return false; }
  lowVoltage=(b[2]&0x80)!=0;
  stopped=(b[0]&0x20)!=0;
  if (lowVoltage) { state="voltage_low"; return false; }
  if (stopped) { state="stopped"; return false; }
  if (b[0]&0x88) { state="test_mode"; return false; }
  if (!mm::decodeRtc(b+2,rtcUtc)) { state="invalid_calendar"; return false; }
  state="valid_calendar";
  return true;
}
}
void rtc8563Begin() {
#if RTC_PCF8563_ENABLED
  busReady=Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN,I2C_BUS_HZ);
  if (busReady) {
    Wire.setTimeOut(I2C_TIMEOUT_MS);
    sample();
  } else state="bus_init_failed";
  Serial.printf("[RTC] PCF8563 addr=0x%02X SDA=%d SCL=%d state=%s utc_us=%llu\n",
    unsigned(ADDRESS),I2C_SDA_PIN,I2C_SCL_PIN,state,(unsigned long long)rtcUtc);
#else
  Serial.println("[RTC] PCF8563 disabled");
#endif
}
void rtc8563Sync(uint64_t utc) {
#if RTC_PCF8563_ENABLED && RTC_WRITE_ON_HOST_SYNC
  if (!busReady || !mm::utcInRange(utc)) return;
  const uint64_t start=esp_timer_get_time();
  sample();
  if (!present) { writeState="not_present"; return; }
  // Explicit host sync only; no writes at boot, no periodic re-setting.
  uint8_t regs[7]={};
  const uint64_t target=utc+uint64_t(esp_timer_get_time())-start;
  if (!mm::encodeRtc(target,regs)) { writeState="invalid_target"; return; }
  // Clear STOP/test bits and set all calendar registers in one transaction.
  // Do not modify alarm/interrupt/CLKOUT/timer configuration.
  const uint8_t run=0;
  if (!writeRegs(0,&run,1) || !writeRegs(2,regs,sizeof(regs))) {
    writeState="write_failed"; sample(); return;
  }
  if (!sample() || rtcUtc<target/1000000ULL*1000000ULL ||
      rtcUtc>target/1000000ULL*1000000ULL+1000000ULL) {
    writeState="readback_failed";
  } else writeState="verified";
  Serial.printf("[RTC] host sync=%s state=%s utc_us=%llu\n",
     writeState,state,(unsigned long long)rtcUtc);
#else
  (void)utc;
  writeState="disabled";
#endif
}
String rtc8563Json(uint64_t trustedUtc) {
  // A fresh sample at diagnostic request; absent module is harmless.
  sample();
  char b[300];
  snprintf(b,sizeof(b),"{\"enabled\":%s,\"bus_ready\":%s,\"present\":%s,\"state\":\"%s\",\"voltage_low\":%s,\"stopped\":%s,\"last_write\":\"%s\",\"utc_us\":",
    RTC_PCF8563_ENABLED?"true":"false",busReady?"true":"false",present?"true":"false",
    state,lowVoltage?"true":"false",stopped?"true":"false",writeState);
  String out(b);
  if (rtcUtc) {
    snprintf(b,sizeof(b),"%llu",(unsigned long long)rtcUtc); out+=b;
  } else out+="null";
  out+=",\"rtc_minus_system_us\":";
  if (rtcUtc && trustedUtc) {
    snprintf(b,sizeof(b),"%lld",(long long)mm::correctionUs(trustedUtc,rtcUtc)); out+=b;
  } else out+="null";
  out+="}";
  return out;
}
