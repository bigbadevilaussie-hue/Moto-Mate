#include "LogCodec.h"
#include <string.h>
#include <stdio.h>

namespace mm { namespace log {
namespace {
void put(uint8_t* p,uint64_t value,size_t n) {
  for(size_t i=0;i<n;++i) { p[i]=uint8_t(value); value>>=8; }
}
bool crcOK(const uint8_t* p,size_t n) { return get(p+n-4,4)==crc32(p,n-4); }
void finish(uint8_t* p,size_t n) { put(p+n-4,crc32(p,n-4),4); }
int64_t signed32(uint64_t u) { return u<=0x7fffffffULL ? int64_t(u) : int64_t(u)-4294967296LL; }
bool presentOrZero(uint16_t v,unsigned bit,const uint8_t* p,size_t n) {
  return (v&(1u<<bit)) || get(p,n)==0;
}
bool start(const Prefix& p,uint8_t* out) {
  if(!out) return false;
  memset(out,0,RECORD_BYTES);
  put(out+0,static_cast<uint64_t>(p.utc_ms),8);
  put(out+8,static_cast<uint64_t>(p.boot_id),4);
  put(out+12,static_cast<uint64_t>(p.mono_ms),4);
  put(out+16,static_cast<uint64_t>(p.type),1);
  put(out+17,static_cast<uint64_t>(p.state),1);
  put(out+18,static_cast<uint64_t>(p.validity),2);
  return true;
}
bool end(uint8_t* out) { finish(out,RECORD_BYTES); return validRecord(out,RECORD_BYTES); }
const char* suffixText(Suffix s) {
  switch(s) {
    case Suffix::Open:return ".open"; case Suffix::Bin:return ".bin";
    case Suffix::Gzip:return ".bin.gz"; case Suffix::GzipTemp:return ".bin.gz.tmp";
    case Suffix::Recovered:return ".rec"; case Suffix::Bad:return ".bad";
  }
  return nullptr;
}
}
uint64_t get(const uint8_t* p,size_t n) {
  uint64_t value=0;
  if(!p || n>8) return 0;
  for(size_t i=0;i<n;++i) value|=uint64_t(p[i])<<(i*8);
  return value;
}
uint32_t crc32(const uint8_t* p,size_t n) {
  uint32_t c=0xffffffffu;
  for(size_t i=0;i<n;++i) {
    c^=p[i];
    for(unsigned b=0;b<8;++b) c=(c>>1)^((c&1u)?0xedb88320u:0u);
  }
  return c^0xffffffffu;
}
bool validHeader(const uint8_t* p,size_t n) {
  if(!p || n!=HEADER_BYTES || memcmp(p,"MMF1",4) || p[4]!=1 ||
     get(p+6,2)!=HEADER_BYTES || get(p+8,2)!=RECORD_BYTES ||
     (get(p+10,2)&~1ULL) || !get(p+20,8) || !get(p+28,8) ||
     get(p+72,4) || !crcOK(p,n)) return false;
  if(p[5]==1) return !get(p+36,8) && !get(p+44,4) && !get(p+48,4) && get(p+60,4)==300000;
  if(p[5]==2) return get(p+36,8) && get(p+44,4) && get(p+48,4) && get(p+60,4)==5000;
  return false;
}
bool encodeHeader(const Header& h,uint8_t* out) {
  if(!out) return false;
  memset(out,0,HEADER_BYTES);
  memcpy(out,"MMF1",4); out[4]=1;
  put(out+6,HEADER_BYTES,2); put(out+8,RECORD_BYTES,2);
  put(out+5,static_cast<uint64_t>(h.kind),1);
  put(out+10,static_cast<uint64_t>(h.flags),2);
  memcpy(out+12,h.device_mac,6);
  put(out+18,static_cast<uint64_t>(h.fw_major),1);
  put(out+19,static_cast<uint64_t>(h.fw_minor),1);
  put(out+20,static_cast<uint64_t>(h.generation),8);
  put(out+28,static_cast<uint64_t>(h.file_seq),8);
  put(out+36,static_cast<uint64_t>(h.trip_seq),8);
  put(out+44,static_cast<uint64_t>(h.leg),4);
  put(out+48,static_cast<uint64_t>(h.part),4);
  put(out+52,static_cast<uint64_t>(h.opened_utc_ms),8);
  put(out+60,static_cast<uint64_t>(h.nominal_period_ms),4);
  put(out+64,static_cast<uint64_t>(h.opened_boot_id),4);
  put(out+68,static_cast<uint64_t>(h.opened_mono_ms),4);
  finish(out,HEADER_BYTES);
  return validHeader(out,HEADER_BYTES);
}
bool validRecord(const uint8_t* p,size_t n) {
  if(!p || n!=RECORD_BYTES || !crcOK(p,n)) return false;
  const uint8_t t=p[16], state=p[17];
  const uint16_t v=uint16_t(get(p+18,2));
  if((state&0x80u) || (state&15u)>9 || (v&0xf800u) ||
     ((v&3u)==0)!=(get(p,8)==0)) return false;
  if(t==1 || t==2) {
    if(!presentOrZero(v,2,p+20,2) || !presentOrZero(v,3,p+22,2) ||
       !presentOrZero(v,4,p+24,2) || !presentOrZero(v,5,p+26,8) ||
       !presentOrZero(v,6,p+34,2) || !presentOrZero(v,7,p+36,2) ||
       !presentOrZero(v,8,p+38,2) || !presentOrZero(v,9,p+40,1) ||
       !presentOrZero(v,10,p+42,2)) return false;
    const int64_t lat=signed32(get(p+26,4)), lon=signed32(get(p+30,4));
    return get(p+24,2)<=10000 && lat>=-900000000 && lat<=900000000 &&
      lon>=-1800000000LL && lon<=1800000000LL && get(p+38,2)<=3599;
  }
  if(t==9) {
    return !(v&~15u) && presentOrZero(v,2,p+24,2) &&
      presentOrZero(v,3,p+26,2) && get(p+20,2)>=1 && get(p+20,2)<=8 &&
      get(p+22,2)<=5 && !get(p+40,4);
  }
  if(v&~3u) return false;
  if(t>=3 && t<=6) {
    const uint64_t reason=get(p+32,2), flags=get(p+34,2);
    return get(p+20,8) && ((t==3 || t==6)==(get(p+28,4)==0)) &&
      reason>=1 && reason<=8 && !(flags&~7ULL) &&
      ((t!=3 && t!=4) || !get(p+36,8));
  }
  switch(t) {
    case 7:return !(get(p+36,4)&~3ULL) && !get(p+40,4);
    case 8: {
      const uint64_t a=get(p+20,8),b=get(p+28,8),c=get(p+36,8);
      if(!a || !b) return c==0x8000000000000000ULL;
      const uint64_t magnitude=a>b?a-b:b-a;
      return magnitude<=0x7fffffffffffffffULL && c==b-a;
    }
    case 10:return get(p+36,2)<=5 && !get(p+40,4);
    case 11:return get(p+40,2)>=1 && get(p+40,2)<=6 && !get(p+42,2);
    case 12:return !((get(p+28,4)|get(p+32,4))&~0x1ffULL);
    default:return false;
  }
}
bool recordMatchesHeader(const uint8_t* h,size_t hn,const uint8_t* r,size_t rn) {
  if(!validHeader(h,hn) || !validRecord(r,rn) ||
     ((h[10]&1u)!=((r[17]>>6)&1u))) return false;
  if(h[5]==1) return true;
  const uint8_t t=r[16];
  if(t==4 || t==5) return get(h+36,8)==get(r+20,8) && get(h+44,4)==get(r+28,4);
  return t==1 || t==11;
}
bool encode(const Prefix& p,const Sample& x,uint8_t* out) {
  if(!(p.type==1 || p.type==2 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.battery_mv),2);
  put(out+22,static_cast<uint64_t>(x.temperature_centi_c),2);
  put(out+24,static_cast<uint64_t>(x.humidity_centi_pct),2);
  put(out+26,static_cast<uint64_t>(x.lat_e7),4);
  put(out+30,static_cast<uint64_t>(x.lon_e7),4);
  put(out+34,static_cast<uint64_t>(x.speed_dkmh),2);
  put(out+36,static_cast<uint64_t>(x.altitude_m),2);
  put(out+38,static_cast<uint64_t>(x.course_ddeg),2);
  put(out+40,static_cast<uint64_t>(x.satellites),1);
  put(out+41,static_cast<uint64_t>(x.sample_flags),1);
  put(out+42,static_cast<uint64_t>(x.gravity_delta_mg),2);
  return end(out);
}
bool encode(const Prefix& p,const Lifecycle& x,uint8_t* out) {
  if(!(p.type>=3 && p.type<=6 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.trip_seq),8);
  put(out+28,static_cast<uint64_t>(x.leg),4);
  put(out+32,static_cast<uint64_t>(x.reason),2);
  put(out+34,static_cast<uint64_t>(x.flags),2);
  put(out+36,static_cast<uint64_t>(x.duration_ms),8);
  return end(out);
}
bool encode(const Prefix& p,const Boot& x,uint8_t* out) {
  if(!(p.type==7 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.reset_reason),2);
  put(out+22,static_cast<uint64_t>(x.wake_cause),2);
  put(out+24,static_cast<uint64_t>(x.prior_awake_us),4);
  put(out+28,static_cast<uint64_t>(x.prior_sleep_ms),4);
  put(out+32,static_cast<uint64_t>(x.lost_total),4);
  put(out+36,static_cast<uint64_t>(x.flags),4);
  return end(out);
}
bool encode(const Prefix& p,const ClockSync& x,uint8_t* out) {
  if(!(p.type==8 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.prior_utc_ms),8);
  put(out+28,static_cast<uint64_t>(x.new_utc_ms),8);
  put(out+36,static_cast<uint64_t>(x.correction_ms),8);
  return end(out);
}
bool encode(const Prefix& p,const Operation& x,uint8_t* out) {
  if(!(p.type==9 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.code),2);
  put(out+22,static_cast<uint64_t>(x.result),2);
  put(out+24,static_cast<uint64_t>(x.battery_mv),2);
  put(out+26,static_cast<uint64_t>(x.temperature_centi_c),2);
  put(out+28,static_cast<uint64_t>(x.detail_code),4);
  put(out+32,static_cast<uint64_t>(x.arg0),4);
  put(out+36,static_cast<uint64_t>(x.arg1),4);
  return end(out);
}
bool encode(const Prefix& p,const Transfer& x,uint8_t* out) {
  if(!(p.type==10 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.file_seq),8);
  put(out+28,static_cast<uint64_t>(x.bytes),4);
  put(out+32,static_cast<uint64_t>(x.duration_ms),4);
  put(out+36,static_cast<uint64_t>(x.result),2);
  put(out+38,static_cast<uint64_t>(x.attempts),2);
  return end(out);
}
bool encode(const Prefix& p,const Gap& x,uint8_t* out) {
  if(!(p.type==11 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.monitor_lost),4);
  put(out+24,static_cast<uint64_t>(x.ride_lost),4);
  put(out+28,static_cast<uint64_t>(x.crank_lost),4);
  put(out+32,static_cast<uint64_t>(x.event_lost),4);
  put(out+36,static_cast<uint64_t>(x.duration_ms),4);
  put(out+40,static_cast<uint64_t>(x.reason),2);
  return end(out);
}
bool encode(const Prefix& p,const WakeSummary& x,uint8_t* out) {
  if(!(p.type==12 ) || !start(p,out)) return false;
  put(out+20,static_cast<uint64_t>(x.awake_us),4);
  put(out+24,static_cast<uint64_t>(x.planned_sleep_ms),4);
  put(out+28,static_cast<uint64_t>(x.completed_mask),4);
  put(out+32,static_cast<uint64_t>(x.deferred_mask),4);
  put(out+36,static_cast<uint64_t>(x.min_heap_bytes),4);
  put(out+40,static_cast<uint64_t>(x.max_loop_gap_us),4);
  return end(out);
}
bool filename(Kind k,uint64_t g,uint64_t seq,Suffix s,char* out) {
  if(!out || !g || !seq) return false;
  const char* tail=suffixText(s);
  if(!tail || (k!=Kind::Monitor && k!=Kind::Ride)) return false;
  const int n=snprintf(out,PATH_BYTES,"/%c1_%016llx%016llx%s",
    k==Kind::Monitor?'m':'r',(unsigned long long)g,(unsigned long long)seq,tail);
  return n>0 && size_t(n)<PATH_BYTES;
}
bool filenameMatchesHeader(const char* path,const uint8_t* h,size_t n) {
  if(!path || !validHeader(h,n)) return false;
  char expected[PATH_BYTES];
  for(unsigned i=0;i<6;++i) {
    if(filename(static_cast<Kind>(h[5]),get(h+20,8),get(h+28,8),static_cast<Suffix>(i),expected)
       && strcmp(path,expected)==0) return true;
  }
  return false;
}
bool encodeIdentity(const Identity& id,uint8_t* out) {
  if(!out || !id.generation || id.reservedThrough%BLOCK_SIZE) return false;
  memcpy(out,"MMI1",4); put(out+4,id.generation,8); put(out+12,id.reservedThrough,8);
  finish(out,ID_BYTES); return true;
}
bool decodeIdentity(const uint8_t* p,size_t n,Identity& id) {
  if(!p || n!=ID_BYTES || memcmp(p,"MMI1",4) || !crcOK(p,n)) return false;
  const Identity x={get(p+4,8),get(p+12,8)};
  if(!x.generation || x.reservedThrough%BLOCK_SIZE) return false;
  id=x; return true;
}
bool nextReservation(const Identity& id,Identity& next,uint64_t& first) {
  if(!id.generation || id.reservedThrough%BLOCK_SIZE || id.reservedThrough>UINT64_MAX-BLOCK_SIZE) return false;
  next={id.generation,id.reservedThrough+BLOCK_SIZE};
  first=id.reservedThrough+1; return true;
}
} }
