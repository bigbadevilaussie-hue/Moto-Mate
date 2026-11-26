#include "../LogSequencePolicy.h"
#include <assert.h>
#include <stdio.h>
using namespace mm::log;
int main() {
  Identity id={123,0}, reserved={}; uint64_t first=0,seq=0;
  assert(nextReservation(id,reserved,first));
  Cursor c={123,first,reserved.reservedThrough};
  assert(cursorMatches(c,reserved));
  uint8_t raw[RETAINED_BYTES]; Cursor restored={};
  assert(takeSequence(c,seq) && seq==1);
  assert(encodeCursor(c,raw) && decodeCursor(raw,sizeof(raw),restored));
  assert(cursorMatches(restored,reserved));
  for(uint64_t i=2;i<=32;i++) assert(takeSequence(restored,seq) && seq==i);
  assert(!takeSequence(restored,seq));
  assert(encodeCursor(restored,raw) && decodeCursor(raw,sizeof(raw),c));
  assert(!takeSequence(c,seq));
  assert(nextReservation(reserved,id,first) && first==33);
  assert(!cursorMatches(c,id));
  c={123,33,64}; assert(encodeCursor(c,raw));
  for(size_t i=0;i<sizeof(raw);i++) {
    raw[i]^=1; assert(!decodeCursor(raw,sizeof(raw),restored)); raw[i]^=1;
  }
  assert(!cursorMatches(c,Identity{124,64}));
  assert(!cursorValid(Cursor{123,32,64}));
  assert(!cursorValid(Cursor{123,66,64}));
  assert(!cursorValid(Cursor{0,33,64}));
  assert(!cursorValid(Cursor{123,1,0}));
  assert(!nextReservation(Identity{123,UINT64_MAX-31},id,first));
  c={123,UINT64_MAX-31,UINT64_MAX-31};
  assert(takeSequence(c,seq) && seq==UINT64_MAX-31);
  assert(!takeSequence(c,seq));
  puts("sequence cursor fixtures and corruption cases PASS");
}
