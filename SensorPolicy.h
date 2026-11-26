#pragma once
#include <stdint.h>
#include <math.h>
namespace mm {
inline uint8_t shtCrc(const uint8_t* bytes) {
  uint8_t crc=0xff;
  for (unsigned i=0;i<2;++i) {
    crc^=bytes[i];
    for (unsigned j=0;j<8;++j) crc=uint8_t((crc&0x80)?(crc<<1)^0x31:crc<<1);
  }
  return crc;
}
inline bool shtDecode(const uint8_t* b,float& t,float& rh) {
  if (shtCrc(b)!=b[2] || shtCrc(b+3)!=b[5]) return false;
  t=-45.0f+175.0f*((uint16_t(b[0])<<8)|b[1])/65535.0f;
  rh=-6.0f+125.0f*((uint16_t(b[3])<<8)|b[4])/65535.0f;
  if (t< -40.0f || t>125.0f) return false;
  rh=fmaxf(0.0f,fminf(100.0f,rh));
  return true;
}
struct Vec3 { float x,y,z; };
inline float norm(Vec3 a) { return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z); }
inline Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 mix(Vec3 a,Vec3 b,float k) {
  return {a.x+(b.x-a.x)*k,a.y+(b.y-a.y)*k,a.z+(b.z-a.z)*k};
}
// Diagnostic only: stationary candidate requires successive stable samples.
// Never adapt an established gravity reference during a detected displacement.
class MotionReference {
public:
  void restore(Vec3 v) {
    if (norm(v)>=0.85f && norm(v)<=1.15f) { ref_=v; ready_=true; }
  }
  void gap() { count_=0; previousValid_=false; }
  void update(Vec3 a,Vec3 gyro) {
    const bool stable=norm(a)>=0.9f && norm(a)<=1.1f && norm(gyro)<3.0f
      && previousValid_ && norm(sub(a,previous_))<0.035f;
    previous_=a; previousValid_=true;
    if (!stable || (ready_ && norm(sub(a,ref_))>0.10f)) { count_=0; return; }
    if (!count_) candidate_=a;
    else candidate_=mix(candidate_,a,1.0f/(count_+1));
    if (count_<20) ++count_;
    if (count_>=20) {
      if (!ready_) { ref_=candidate_; ready_=true; }
      else ref_=mix(ref_,a,0.005f);
    }
  }
  bool ready() const { return ready_; }
  Vec3 reference() const { return ref_; }
  float delta(Vec3 a) const { return ready_?norm(sub(a,ref_)):0; }
private:
  Vec3 ref_={},previous_={},candidate_={};
  unsigned count_=0;
  bool ready_=false,previousValid_=false;
};
}
