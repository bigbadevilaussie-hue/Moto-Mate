#pragma once
#include "LogCodec.h"
#include "LogSleepPolicy.h"
#include <string.h>
namespace mm { namespace log {
// RTC-only authorization to resume a verified open file after a qualified timer wake.
// Byte layout: encoded header, count (LE u32), CRC32. No persistent extra NVS.
constexpr size_t SLEEP_CHECKPOINT_BYTES=HEADER_BYTES+8;
inline bool encodeSleepCheckpoint(const Header& h,unsigned count,uint8_t* b) {
  if(!b || count<1 || count>=SLEEP_LOG_CYCLES || h.kind!=1 || h.flags!=1 || !encodeHeader(h,b)) return false;
  b[80]=uint8_t(count); b[81]=b[82]=b[83]=0;
  const uint32_t c=crc32(b,84);
  for(unsigned i=0;i<4;++i) b[84+i]=uint8_t(c>>(8*i));
  return true;
}
inline bool decodeSleepCheckpoint(const uint8_t* b,size_t n,Header& h,unsigned& count) {
  if(!b || n!=SLEEP_CHECKPOINT_BYTES || !validHeader(b,HEADER_BYTES) ||
     b[5]!=1 || get(b+10,2)!=1 || get(b+84,4)!=crc32(b,84) ||
     get(b+80,4)<1 || get(b+80,4)>=SLEEP_LOG_CYCLES) return false;
  Header x={};
  x.kind=1; x.flags=1; memcpy(x.device_mac,b+12,6);
  x.fw_major=b[18]; x.fw_minor=b[19];
  x.generation=get(b+20,8); x.file_seq=get(b+28,8);
  x.opened_utc_ms=get(b+52,8); x.nominal_period_ms=uint32_t(get(b+60,4));
  x.opened_boot_id=uint32_t(get(b+64,4)); x.opened_mono_ms=uint32_t(get(b+68,4));
  uint8_t check[HEADER_BYTES];
  if(!encodeHeader(x,check) || memcmp(check,b,HEADER_BYTES)) return false;
  h=x; count=unsigned(get(b+80,4)); return true;
}
} }
