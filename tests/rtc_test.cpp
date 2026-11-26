#include "RtcPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  uint8_t b[7]={};
  uint64_t t=0;
  const uint64_t leap=1709164800ULL*1000000ULL;
  assert(mm::encodeRtc(leap,b));
  assert(b[0]==0 && b[1]==0 && b[2]==0 && b[3]==0x29 && b[4]==4 && b[5]==2 && b[6]==0x24);
  assert(mm::decodeRtc(b,t) && t==leap);
  b[0]|=0x80; assert(!mm::decodeRtc(b,t));
  b[0]=0x6a; assert(!mm::decodeRtc(b,t));
  assert(mm::encodeRtc(leap,b)); b[3]=0x30; assert(!mm::decodeRtc(b,t));
  assert(mm::encodeRtc(leap,b)); b[6]=0x23; assert(!mm::decodeRtc(b,t));
  assert(mm::encodeRtc(leap,b)); b[4]=6; assert(!mm::decodeRtc(b,t));
  assert(mm::encodeRtc(mm::UTC_MAX_US,b)); assert(mm::decodeRtc(b,t) && t==mm::UTC_MAX_US);
  assert(!mm::encodeRtc(mm::UTC_MAX_US+1,b));
  puts("PASS: PCF8563 BCD, leap date, weekday, voltage flag and UTC bounds");
}
