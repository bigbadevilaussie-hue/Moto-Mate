#include "SettingsPolicy.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  mm::Settings s;
  assert(mm::settingsValid(s));
  assert(mm::encode(s)==mm::SETTINGS_V1);
  for (uint32_t flags=0;flags<16;++flags) {
    mm::Settings v;
    const bool valid=!(flags&8u)||((flags&3u)==2u);
    assert(mm::decode(mm::SETTINGS_V1|flags,v)==valid);
    if (!valid) continue;
    assert(mm::encode(v)==(mm::SETTINGS_V1|flags));
    assert(!mm::fieldActive(v));
    assert(mm::benchSleepActive(v)==((flags&1)==0 && (flags&2)!=0));
    assert(mm::verboseActive(v)==((flags&1)==0 && (flags&4)!=0));
    assert(v.engine_start_mv==13200 && v.adc_calibrated==0);
  }
  assert(!mm::decode(0,s));
  assert(!mm::decode(mm::SETTINGS_V1|16,s));
  assert(!mm::decode(mm::SETTINGS_V2,s));
  uint8_t bytes[mm::SETTINGS_BYTES],again[mm::SETTINGS_BYTES];
  s.adc_offset_mv=-137; s.fake_alarm_enabled=0; s.mpu_enabled=0;
  s.benchSleep=true; s.benchSleepLog=true;
  assert(mm::encodeV2(s,bytes,sizeof(bytes)));
  mm::Settings v;
  assert(mm::decodeV2(bytes,sizeof(bytes),v));
  assert(v.adc_offset_mv==-137 && v.fake_alarm_enabled==0 && !v.mpu_enabled && v.benchSleepLog);
  assert(mm::encodeV2(v,again,sizeof(again)));
  assert(!memcmp(bytes,again,sizeof(bytes)));
  for (size_t i=0;i<sizeof(bytes);++i) {
    for (unsigned bit=0;bit<8;++bit) {
      bytes[i]^=uint8_t(1u<<bit);
      assert(!mm::decodeV2(bytes,sizeof(bytes),v));
      assert(v.adc_offset_mv==-137);
      bytes[i]^=uint8_t(1u<<bit);
    }
    assert(!mm::decodeV2(bytes,i,v));
  }
#define MM_TEST(name, def, lo, hi) \
  v=mm::Settings{}; v.name=(lo)-1; assert(!mm::settingsValid(v)); \
  v=mm::Settings{}; v.name=(hi)+1; assert(!mm::settingsValid(v));
  MM_SETTINGS_FIELDS(MM_TEST)
#undef MM_TEST
  v=mm::Settings{}; v.engine_stop_mv=v.engine_start_mv; assert(!mm::settingsValid(v));
  v=mm::Settings{}; v.agm_warning_mv=v.agm_panic_mv; assert(!mm::settingsValid(v));
  v=mm::Settings{}; v.agm_safe_mv=v.agm_warning_mv; assert(!mm::settingsValid(v));
  v=mm::Settings{}; v.charge_high_mv=v.charge_low_mv; assert(!mm::settingsValid(v));
  v=mm::Settings{}; v.gps_fix_warn_ms=1000; assert(!mm::settingsValid(v));
  v=mm::Settings{}; v.profile=mm::Profile::Field; v.benchSleepLog=true; assert(!mm::settingsValid(v));
  assert(!mm::encodeV2(v,again,sizeof(again)));
  // Valid CRC cannot legitimise an out-of-range or unsupported record.
  memcpy(again,bytes,sizeof(bytes)); mm::putU32(again+8,0);
  mm::putU32(again+sizeof(again)-4,mm::settingsCrc(again,sizeof(again)-4));
  assert(!mm::decodeV2(again,sizeof(again),v));
  memcpy(again,bytes,sizeof(bytes)); mm::putU32(again,0x4d4d0300U);
  mm::putU32(again+sizeof(again)-4,mm::settingsCrc(again,sizeof(again)-4));
  assert(!mm::decodeV2(again,sizeof(again),v));
  const uint8_t known[]={'1','2','3','4','5','6','7','8','9'};
  assert(mm::settingsCrc(known,sizeof(known))==0xcbf43926U);
  printf("PASS: settings v2, %zu fields, %zu bytes, migration codec, validation, corruption, readiness\n",mm::SETTINGS_FIELD_COUNT,mm::SETTINGS_BYTES);
}
