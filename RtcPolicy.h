#pragma once
#include "ClockPolicy.h"
namespace mm {
inline bool leapYear(unsigned y) { return y%4==0 && (y%100!=0 || y%400==0); }
inline unsigned monthDays(unsigned y,unsigned m) {
  const unsigned d[]={31,28,31,30,31,30,31,31,30,31,30,31};
  return m>=1 && m<=12 ? d[m-1]+(m==2 && leapYear(y)?1:0):0;
}
inline bool bcd(uint8_t raw,unsigned limit,unsigned& v) {
  if ((raw&15)>9 || (raw>>4)>9) return false;
  v=(raw>>4)*10+(raw&15);
  return v<=limit;
}
inline uint8_t toBcd(unsigned v) { return uint8_t((v/10)*16+v%10); }
// Registers 02h..08h. Century flag is a rollover indicator with
// software-defined interpretation; commissioning uses 2000..2099.
inline bool decodeRtc(const uint8_t* b,uint64_t& utc) {
  unsigned s,m,h,d,w,mo,y;
  if ((b[0]&0x80) || !bcd(b[0]&0x7f,59,s) || !bcd(b[1]&0x7f,59,m)
      || !bcd(b[2]&0x3f,23,h) || !bcd(b[3]&0x3f,31,d)
      || !bcd(b[4]&7,6,w) || !bcd(b[5]&0x1f,12,mo)
      || !bcd(b[6],99,y)) return false;
  y+=2000;
  if (!d || !mo || d>monthDays(y,mo)) return false;
  uint64_t days=0;
  for (unsigned i=1970;i<y;++i) days+=leapYear(i)?366:365;
  for (unsigned i=1;i<mo;++i) days+=monthDays(y,i);
  days+=d-1;
  if ((days+4)%7!=w) return false;
  const uint64_t candidate=(days*86400ULL+h*3600ULL+m*60ULL+s)*1000000ULL;
  if (!utcInRange(candidate)) return false;
  utc=candidate; return true;
}
inline bool encodeRtc(uint64_t utc,uint8_t* b) {
  if (!utcInRange(utc)) return false;
  uint64_t seconds=utc/1000000ULL, days=seconds/86400ULL;
  unsigned y=1970, mo=1;
  const unsigned weekday=(days+4)%7;
  while (days>=unsigned(leapYear(y)?366:365)) { days-=leapYear(y)?366:365; ++y; }
  while (days>=monthDays(y,mo)) { days-=monthDays(y,mo); ++mo; }
  b[0]=toBcd(seconds%60); b[1]=toBcd(seconds/60%60);
  b[2]=toBcd(seconds/3600%24); b[3]=toBcd(unsigned(days)+1);
  b[4]=uint8_t(weekday); b[5]=toBcd(mo); b[6]=toBcd(y-2000);
  return true;
}
}
