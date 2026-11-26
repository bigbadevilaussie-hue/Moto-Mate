#include "../LogSleepCheckpoint.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace mm::log;
  Header h={}; h.kind=1; h.flags=1; h.fw_major=1; h.fw_minor=33;
  h.generation=7; h.file_seq=161; h.nominal_period_ms=300000;
  h.opened_boot_id=123; h.opened_mono_ms=3000;
  uint8_t b[SLEEP_CHECKPOINT_BYTES]; Header out={}; unsigned count=0;
  assert(!encodeSleepCheckpoint(h,0,b) && !encodeSleepCheckpoint(h,3,b));
  for(unsigned i=1;i<=2;++i) {
    assert(encodeSleepCheckpoint(h,i,b));
    assert(decodeSleepCheckpoint(b,sizeof(b),out,count) && count==i && out.file_seq==161 && out.opened_boot_id==123);
    for(size_t j=0;j<sizeof(b);++j) {
      b[j]^=1; assert(!decodeSleepCheckpoint(b,sizeof(b),out,count)); b[j]^=1;
    }
  }
  puts("PASS: retained sleep checkpoint roundtrip and every-byte corruption rejection");
}
