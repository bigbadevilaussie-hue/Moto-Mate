#include "BenchSensors.h"
#include "SensorPolicy.h"
#include "I2cBus.h"
#include "Config.h"
#include <esp_attr.h>
#include <esp_timer.h>
namespace {
constexpr uint64_t RETRY=5000000ULL;
constexpr uint32_t REF_MAGIC=0x4d4d0123;
RTC_DATA_ATTR uint32_t refMagic=0;
RTC_DATA_ATTR mm::Vec3 savedRef={};
bool enabled=false,paused=false,shtPending=false,shtValid=false,mpuReady=false,mpuValid=false,mpuAwakePossible=false;
const char* shtState="disabled";
const char* mpuState="disabled";
uint64_t shtNext=0,shtReadAt=0,shtAt=0,mpuNext=0,mpuAt=0;
float temp=0,rh=0;
mm::Vec3 accel={},gyro={};
mm::MotionReference reference;
bool regWrite(uint8_t reg,uint8_t val) {
  const uint8_t data[]={reg,val};
  return i2cWrite(MPU6050_ADDRESS,data,2);
}
bool mpuSetup() {
  const uint8_t probe=i2cProbe(MPU6050_ADDRESS);
  if (probe) { mpuState=probe==2?"absent":"probe_error"; return false; }
  uint8_t who=0;
  if (!i2cReadRegs(MPU6050_ADDRESS,0x75,&who,1)) { mpuState="read_error"; return false; }
  if (who!=0x68) { mpuState="wrong_id"; return false; }
  mpuAwakePossible=true;
  // +/-2g, +/-250 degrees/s, filtered bandwidth, 50 Hz output.
  if (!regWrite(0x6b,0x01) || !regWrite(0x6c,0x00)
      || !regWrite(0x1a,0x03) || !regWrite(0x19,19)
      || !regWrite(0x1b,0) || !regWrite(0x1c,0)) {
    mpuState="config_error"; return false;
  }
  uint8_t cfg[4]={},power[2]={};
  if (!i2cReadRegs(MPU6050_ADDRESS,0x19,cfg,4) ||
      !i2cReadRegs(MPU6050_ADDRESS,0x6b,power,2) ||
      cfg[0]!=19 || cfg[1]!=3 || cfg[2]!=0 || cfg[3]!=0 || power[0]!=1 || power[1]!=0) {
    mpuState="config_readback_failed"; return false;
  }
  mpuState="warming";
  return true;
}
int16_t raw(const uint8_t* b) { return int16_t((uint16_t(b[0])<<8)|b[1]); }
void shtPoll(uint64_t now) {
#if SHT40_ENABLED
  if (shtPending) {
    if (now<shtReadAt) return;
    shtPending=false;
    uint8_t b[6]={};
    shtValid=i2cRead(SHT40_ADDRESS,b,6) && mm::shtDecode(b,temp,rh);
    shtState=shtValid?"valid":"read_or_crc_error";
    if (shtValid) shtAt=esp_timer_get_time();
    shtNext=now+(shtValid?2000000ULL:RETRY);
    return;
  }
  if (now<shtNext) return;
  const uint8_t probe=i2cProbe(SHT40_ADDRESS);
  if (probe) {
    shtValid=false; shtState=probe==2?"absent":"probe_error"; shtNext=now+RETRY; return;
  }
  const uint8_t cmd=0xfd;
  if (!i2cWrite(SHT40_ADDRESS,&cmd,1)) {
    shtValid=false; shtState="command_error"; shtNext=now+RETRY; return;
  }
  shtPending=true; shtReadAt=uint64_t(esp_timer_get_time())+10000;
#endif
}
void mpuPoll(uint64_t now) {
#if MPU6050_ENABLED
  if (now<mpuNext) return;
  if (!mpuReady) {
    mpuReady=mpuSetup(); mpuValid=false; reference.gap();
    mpuNext=uint64_t(esp_timer_get_time())+(mpuReady?100000ULL:RETRY);
    return;
  }
  uint8_t b[14]={};
  if (!i2cReadRegs(MPU6050_ADDRESS,0x3b,b,14)) {
    mpuValid=mpuReady=false; mpuState="read_error"; reference.gap(); mpuNext=now+RETRY; return;
  }
  accel={raw(b)/16384.0f,raw(b+2)/16384.0f,raw(b+4)/16384.0f};
  gyro={raw(b+8)/131.0f,raw(b+10)/131.0f,raw(b+12)/131.0f};
  mpuValid=true; mpuAt=esp_timer_get_time(); mpuState="valid";
  reference.update(accel,gyro);
  mpuNext=now+100000ULL;
#endif
}
const char* flag(bool b) { return b?"true":"false"; }
String number(bool valid,float v) { return valid?String(v,4):String("null"); }
String vector(bool valid,mm::Vec3 v) {
  if (!valid) return "null";
  return String("[")+String(v.x,4)+","+String(v.y,4)+","+String(v.z,4)+"]";
}
}
void sensorsBegin(bool deepSleepWake) {
  enabled=i2cBegin();
  if (deepSleepWake && refMagic==REF_MAGIC) reference.restore(savedRef);
  if (!reference.ready()) refMagic=0;
  if (SHT40_ENABLED) shtState=enabled?"pending":"bus_init_failed";
  if (MPU6050_ENABLED) mpuState=enabled?"pending":"bus_init_failed";
  Serial.printf("[SENSORS] SHT40=0x%02X MPU6050=0x%02X bus=%s reference=%s\n",
    SHT40_ADDRESS,MPU6050_ADDRESS,enabled?"ready":"failed",reference.ready()?"retained":"learning");
}
void sensorsPoll(uint64_t now) {
  if (!enabled || paused) return;
  // At most one device transaction sequence per iteration where possible.
  shtPoll(now);
  mpuPoll(uint64_t(esp_timer_get_time()));
}
bool sensorsQuiesce() {
  if (paused) return !mpuAwakePossible;
  paused=true; shtPending=false; shtValid=mpuValid=false; reference.gap();
  if (reference.ready()) { savedRef=reference.reference(); refMagic=REF_MAGIC; }
  if (mpuAwakePossible) {
    uint8_t p=0;
    if (regWrite(0x6b,0x40) && i2cReadRegs(MPU6050_ADDRESS,0x6b,&p,1) && (p&0x40)) {
      mpuAwakePossible=false; mpuState="sleep_verified";
    } else mpuState="sleep_unverified";
  }
  mpuReady=false;
  Serial.printf("[SENSORS] quiesced MPU=%s\n",mpuState);
  return !mpuAwakePossible;
}
void sensorsResume() {
  paused=false; shtNext=mpuNext=0;
}
String sensorsJson() {
  const uint64_t now=esp_timer_get_time();
  const bool sv=shtValid && now-shtAt<=3000000ULL;
  const bool mv=mpuValid && now-mpuAt<=500000ULL;
  String s=String("{\"sht40\":{\"enabled\":")+flag(SHT40_ENABLED)+",\"state\":\""+shtState+"\",\"valid\":"+flag(sv);
  s+=",\"temperature_c\":"+number(sv,temp)+",\"humidity_pct\":"+number(sv,rh)+"}";
  s+=String(",\"mpu6050\":{\"enabled\":")+flag(MPU6050_ENABLED)+",\"state\":\""+mpuState+"\",\"valid\":"+flag(mv);
  s+=",\"accel_g\":"+vector(mv,accel)+",\"gyro_dps\":"+vector(mv,gyro);
  s+=String(",\"reference_ready\":")+flag(reference.ready())+",\"gravity_reference_g\":"+vector(reference.ready(),reference.reference());
  s+=",\"gravity_delta_g\":"+number(mv&&reference.ready(),reference.delta(accel))+"}}";
  return s;
}

void sensorsLogSample(uint64_t now,mm::log::Sample& sample,uint16_t& validity) {
  if(shtValid && now>=shtAt && now-shtAt<=3000000ULL &&
     isfinite(temp) && isfinite(rh) && temp>=-40 && temp<=125 && rh>=0 && rh<=100) {
    sample.temperature_centi_c=int16_t(lroundf(temp*100));
    sample.humidity_centi_pct=uint16_t(lroundf(rh*100));
    validity|=(1u<<3)|(1u<<4);
  }
  if(mpuValid && now>=mpuAt && now-mpuAt<=500000ULL && reference.ready()) {
    const float delta=reference.delta(accel);
    if(isfinite(delta) && delta>=0 && delta<=65.535f) {
      sample.gravity_delta_mg=uint16_t(lroundf(delta*1000));
      validity|=1u<<10;
    }
  }
}
