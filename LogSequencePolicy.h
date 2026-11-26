#pragma once
#include "LogCodec.h"
namespace mm { namespace log {
// Explicit byte representation in RTC memory: MMR1, generation, next, last,
// CRC32. Never trust it unless wake continuity and durable high-water agree.
constexpr size_t RETAINED_BYTES=32;
struct Cursor { uint64_t generation,next,last; };
inline void cursorPut(uint8_t* p,uint64_t v,size_t n) {
  for(size_t i=0;i<n;++i) { p[i]=uint8_t(v); v>>=8; }
}
inline bool cursorValid(const Cursor& c) {
  return c.generation && c.last>=BLOCK_SIZE && c.last%BLOCK_SIZE==0 &&
    c.next>c.last-BLOCK_SIZE && c.next<=c.last+1;
}
inline bool encodeCursor(const Cursor& c,uint8_t* b) {
  if(!b || !cursorValid(c)) return false;
  b[0]='M'; b[1]='M'; b[2]='R'; b[3]='1';
  cursorPut(b+4,c.generation,8); cursorPut(b+12,c.next,8);
  cursorPut(b+20,c.last,8); cursorPut(b+28,crc32(b,28),4); return true;
}
inline bool decodeCursor(const uint8_t* b,size_t n,Cursor& c) {
  if(!b || n!=RETAINED_BYTES || b[0]!='M' || b[1]!='M' ||
    b[2]!='R' || b[3]!='1' || get(b+28,4)!=crc32(b,28)) return false;
  Cursor x={get(b+4,8),get(b+12,8),get(b+20,8)};
  if(!cursorValid(x)) return false;
  c=x; return true;
}
inline bool cursorMatches(const Cursor& c,const Identity& id) {
  return cursorValid(c) && c.generation==id.generation && c.last==id.reservedThrough;
}
inline bool takeSequence(Cursor& c,uint64_t& seq) {
  if(!cursorValid(c) || c.next>c.last) return false;
  seq=c.next++; return true;
}
} }
