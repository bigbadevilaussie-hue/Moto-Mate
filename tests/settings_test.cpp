#include "SettingsPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  mm::Settings defaults;
  assert(mm::encode(defaults)==mm::SETTINGS_V1);
  for (uint32_t flags=0;flags<8;++flags) {
    mm::Settings s;
    assert(mm::decode(mm::SETTINGS_V1|flags,s));
    assert(mm::encode(s)==(mm::SETTINGS_V1|flags));
    assert(!mm::fieldActive(s) && !mm::benchSleepActive(s));
    assert(mm::verboseActive(s)==((flags&1)==0 && (flags&4)!=0));
  }
  mm::Settings s;
  assert(!mm::decode(0,s));
  assert(!mm::decode(mm::SETTINGS_V1|8,s));
  assert(!mm::decode(0x4d4d0200,s));
  assert(mm::encode(s)==mm::SETTINGS_V1);
  puts("PASS: settings schema, all flags, invalid data and readiness gates");
}
