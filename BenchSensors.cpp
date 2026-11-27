#include "BenchSensors.h"
#include "SensorPolicy.h"
#include "I2cBus.h"
#include "Config.h"
#include <esp_attr.h>
#include <esp_timer.h>
#include <initializer_list>
namespace {
constexpr uint64_t RETRY=2000000ULL;
constexpr uint64_t DIAG_PERIOD=2000000ULL;
uint64_t diagNext=0;
constexpr uint32_t REF_MAGIC=0x4d4d0123;
RTC_DATA_ATTR uint32_t refMagic=0;
RTC_DATA_ATTR mm::Vec3 savedRef={};
bool enabled=false,paused=false,shtPending=false,shtValid=false,mpuReady=false,mpuValid=false,mpuAwakePossible=false;
const char* shtState="disabled";
const char* mpuState="disabled";
uint8_t mpuAddress=MPU6050_ADDRESS, mpuWho=0;
// Linux inv_mpu6050 identifies 0x74 as MPU6515 (MPU6500 register map).
// Only these explicitly supported identities may be configured.
bool supportedMpu(uint8_t id) { return id==0x68 || id==0x74; }
const char* mpuModel() { return mpuWho==0x68?"MPU6050":mpuWho==0x74?"MPU6515-compatible":"unknown"; }
bool mpuAddressLocked=false, shtReported=false, mpuReported=false;
uint64_t shtNext=0,shtReadAt=0,shtAt=0,mpuNext=0,mpuAt=0;
float temp=0,rh=0;
mm::Vec3 accel={},gyro={};
mm::MotionReference reference;
void diagnoseBus() {
  // Address-only probes: bounded known devices, no broad scan on a stuck bus.
  Serial.printf("[I2C] SDA=GPIO%d level=%d SCL=GPIO%d level=%d clock=%lu Hz\n",
    I2C_SDA_PIN,digitalRead(I2C_SDA_PIN),I2C_SCL_PIN,digitalRead(I2C_SCL_PIN),(unsigned long)I2C_BUS_HZ);
  for (uint8_t address : {uint8_t(SHT40_ADDRESS),uint8_t(RTC_PCF8563_ADDRESS),uint8_t(0x68),uint8_t(0x69)}) {
    const uint8_t result=i2cProbe(address);
    Serial.printf("[I2C] addr=0x%02X result=%u %s",
      unsigned(address),unsigned(result),result==0?"ACK":result==2?"NACK":"BUS ERROR");
    if (result==0 && (address==0x68 || address==0x69)) {
      uint8_t who=0;
      if (i2cReadRegs(address,0x75,&who,1)) Serial.printf(" WHO_AM_I=0x%02X",unsigned(who));
      else Serial.print(" WHO_AM_I read failed");
    }
    Serial.println();
  }
}
bool regWrite(uint8_t reg,uint8_t val) {
  const uint8_t data[]={reg,val};
  return i2cWrite(mpuAddress,data,2);
}
bool mpuSetup() {
  uint8_t probe=i2cProbe(mpuAddress);
  // Only select the alternate on a clean NACK before any configuration writes.
  // Once selected, never switch away from a possibly awake MPU.
  if (!mpuAddressLocked && probe==2 && mpuAddress==MPU6050_ADDRESS) {
    const uint8_t alternate=MPU6050_ADDRESS==0x68?0x69:0x68;
    if (i2cProbe(alternate)==0) {
      uint8_t id=0;
      if (i2cReadRegs(alternate,0x75,&id,1) && supportedMpu(id)) {
        mpuAddress=alternate;
        probe=0;
        Serial.printf("[MYPOO] alternate address 0x%02X verified; no AD0 wiring change\n",unsigned(mpuAddress));
      }
    }
  }
  if (probe) { mpuState=probe==2?"absent":"probe_error"; return false; }
  uint8_t who=0;
  if (!i2cReadRegs(mpuAddress,0x75,&who,1)) { mpuState="read_error"; return false; }
  mpuWho=who;
  if (!supportedMpu(who)) { mpuState="wrong_id"; return false; }
  mpuAddressLocked=true;
  mpuAwakePossible=true;
  // +/-2g, +/-250 degrees/s, filtered bandwidth, 50 Hz output.
  if (!regWrite(0x6b,0x01) || !regWrite(0x6c,0x00)
      || !regWrite(0x1a,0x03) || !regWrite(0x19,19)
      || !regWrite(0x1b,0) || !regWrite(0x1c,0)
      || (who==0x74 && !regWrite(0x1d,0x03))) {
    mpuState="config_error"; return false;
  }
  uint8_t cfg[4]={},power[2]={};
  if (!i2cReadRegs(mpuAddress,0x19,cfg,4) ||
      !i2cReadRegs(mpuAddress,0x6b,power,2) ||
      cfg[0]!=19 || cfg[1]!=3 || cfg[2]!=0 || cfg[3]!=0 || power[0]!=1 || power[1]!=0) {
    mpuState="config_readback_failed"; return false;
  }
  if (who==0x74) {
    uint8_t accelCfg2=0;
    if (!i2cReadRegs(mpuAddress,0x1d,&accelCfg2,1) || (accelCfg2&0x0f)!=0x03) {
      mpuState="config_readback_failed"; return false;
    }
  }
  Serial.printf("[MYPOO] identified %s WHO_AM_I=0x%02X configuration verified\n",mpuModel(),unsigned(mpuWho));
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
    if (shtValid) {
      shtAt=esp_timer_get_time();
      if (!shtReported) {
        Serial.printf("[SHIT40] PASS temperature=%.2f C humidity=%.2f %% CRC=OK\n",double(temp),double(rh));
        shtReported=true;
      }
    }
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
    Serial.printf("[MYPOO] addr=0x%02X setup=%s\n",unsigned(mpuAddress),mpuState);
    mpuNext=uint64_t(esp_timer_get_time())+(mpuReady?100000ULL:RETRY);
    return;
  }
  uint8_t b[14]={};
  if (!i2cReadRegs(mpuAddress,0x3b,b,14)) {
    mpuValid=mpuReady=false; mpuState="read_error"; reference.gap(); mpuNext=now+RETRY; return;
  }
  accel={raw(b)/16384.0f,raw(b+2)/16384.0f,raw(b+4)/16384.0f};
  gyro={raw(b+8)/131.0f,raw(b+10)/131.0f,raw(b+12)/131.0f};
  mpuValid=true; mpuAt=esp_timer_get_time(); mpuState="valid";
  reference.update(accel,gyro);
  if (!mpuReported) {
    Serial.printf("[MYPOO] PASS addr=0x%02X accel_g=%.3f,%.3f,%.3f gyro_dps=%.2f,%.2f,%.2f\n",unsigned(mpuAddress),double(accel.x),double(accel.y),double(accel.z),double(gyro.x),double(gyro.y),double(gyro.z));
    mpuReported=true;
  }
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
  if (enabled) {
    diagnoseBus();
    diagNext=uint64_t(esp_timer_get_time())+DIAG_PERIOD;
  }
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
  const uint64_t t=esp_timer_get_time();
  // Never address-probe SHT40 during an outstanding measurement.
  if (!shtPending && t>=diagNext) {
    diagNext=t+DIAG_PERIOD;
    diagnoseBus();
    if (shtValid && t>=shtAt && t-shtAt<=3000000ULL)
      Serial.printf("[SHIT40] state=%s temperature=%.2f C humidity=%.2f %% CRC=OK\n",shtState,double(temp),double(rh));
    else Serial.printf("[SHIT40] state=%s no fresh reading\n",shtState);
    if (mpuValid && t>=mpuAt && t-mpuAt<=500000ULL)
      Serial.printf("[MYPOO] state=%s addr=0x%02X accel_g=%.3f,%.3f,%.3f gyro_dps=%.2f,%.2f,%.2f\n",mpuState,unsigned(mpuAddress),double(accel.x),double(accel.y),double(accel.z),double(gyro.x),double(gyro.y),double(gyro.z));
    else Serial.printf("[MYPOO] state=%s addr=0x%02X no fresh reading\n",mpuState,unsigned(mpuAddress));
  }
}
bool sensorsQuiesce() {
  if (paused) return !mpuAwakePossible;
  paused=true; shtPending=false; shtValid=mpuValid=false; reference.gap();
  if (reference.ready()) { savedRef=reference.reference(); refMagic=REF_MAGIC; }
  if (mpuAwakePossible) {
    uint8_t p=0;
    if (regWrite(0x6b,0x40) && i2cReadRegs(mpuAddress,0x6b,&p,1) && (p&0x40)) {
      mpuAwakePossible=false; mpuState="sleep_verified";
    } else mpuState="sleep_unverified";
  }
  mpuReady=false;
  Serial.printf("[SENSORS] quiesced MPU=%s SHT40=%s\n",mpuState,shtState);
  return !mpuAwakePossible;
}
void sensorsResume() {
  paused=false; shtNext=mpuNext=diagNext=0;
}
void sensorsBenchEnvironment(uint64_t now,float& temperature,float& humidity) {
  const bool fresh=shtValid && now>=shtAt && now-shtAt<=3000000ULL;
  temperature=fresh?temp:NAN; humidity=fresh?rh:NAN;
}
String sensorsJson() {
  const uint64_t now=esp_timer_get_time();
  const bool sv=shtValid && now-shtAt<=3000000ULL;
  const bool mv=mpuValid && now-mpuAt<=500000ULL;
  String s=String("{\"sht40\":{\"enabled\":")+flag(SHT40_ENABLED)+",\"state\":\""+shtState+"\",\"valid\":"+flag(sv);
  s+=",\"temperature_c\":"+number(sv,temp)+",\"humidity_pct\":"+number(sv,rh)+"}";
  s+=String(",\"mpu6050\":{\"enabled\":")+flag(MPU6050_ENABLED)+",\"state\":\""+mpuState+"\",\"valid\":"+flag(mv);
  s+=",\"address\":"+String(unsigned(mpuAddress));
  s+=",\"who_am_i\":"+String(unsigned(mpuWho));
  s+=String(",\"model\":\"")+mpuModel()+"\"";
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
