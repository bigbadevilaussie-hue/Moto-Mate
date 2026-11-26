#include "../LogInventoryPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace mm::log;
  uint64_t g=0,s=0; Suffix k=Suffix::Bad;
  char p[PATH_BYTES];
  for(unsigned i=0;i<6;++i) {
    assert(filename(Kind::Monitor,7,161,static_cast<Suffix>(i),p));
    assert(parseMonitorName(p+1,g,s,k) && g==7 && s==161 && unsigned(k)==i);
  }
  assert(!parseMonitorName("m1_000000000000000700000000000000A1.bin",g,s,k));
  assert(!parseMonitorName("m1_00000000000000070000000000000000.bin",g,s,k));
  assert(!parseMonitorName("m1_000000000000000000000000000000a1.bin",g,s,k));
  assert(!parseMonitorName("m1_000000000000000700000000000000a1.bin.extra",g,s,k));
  assert(!parseMonitorName("r1_000000000000000700000000000000a1.open",g,s,k));
  assert(!parseMonitorName("m1_",g,s,k));
  puts("PASS: canonical monitor inventory filenames and rejection cases");
}
