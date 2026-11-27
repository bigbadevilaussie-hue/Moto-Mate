#pragma once
#include <stdint.h>
#include <stddef.h>

// Schema order is on-disk ABI: never reorder without a new schema/migration.
// name, factory default, inclusive minimum, inclusive maximum; integer units.
#define MM_SETTINGS_FIELDS(X) \
  X(bench_awake_ms, 30000, 1000, 300000) \
  X(bench_sleep_ms, 30000, 1000, 3600000) \
  X(field_sleep_ms, 300000, 60000, 3600000) \
  X(engine_start_mv, 13200, 11000, 15500) \
  X(engine_stop_mv, 12900, 10000, 15000) \
  X(agm_safe_mv, 12600, 11000, 14000) \
  X(agm_warning_mv, 12300, 10000, 13900) \
  X(agm_panic_mv, 12000, 9000, 13800) \
  X(charge_low_mv, 13200, 11000, 15000) \
  X(charge_high_mv, 14800, 12000, 16000) \
  X(rider_ready_ms, 30000, 1000, 120000) \
  X(battery_test_ms, 10000, 1000, 30000) \
  X(double_click_ms, 350, 100, 800) \
  X(long_press_ms, 2000, 1000, 5000) \
  X(restart_grace_ms, 30000, 1000, 120000) \
  X(engine_start_chirp, 1, 0, 1) \
  X(fake_alarm_enabled, 1, 0, 1) \
  X(post_ride_check_enabled, 1, 0, 1) \
  X(post_ride_delay_ms, 10000, 1000, 60000) \
  X(adc_gain_ppm, 1000000, 500000, 1500000) \
  X(adc_offset_mv, 0, -3000, 3000) \
  X(adc_calibrated, 0, 0, 1) \
  X(mpu_enabled, 1, 0, 1) \
  X(mpu_movement_mg, 150, 20, 2000) \
  X(mpu_tilt_deg, 10, 1, 90) \
  X(mpu_qualify_ms, 500, 50, 10000) \
  X(mpu_ride_hz, 50, 1, 100) \
  X(mpu_auto_calibrate, 1, 0, 1) \
  X(gps_enabled, 1, 0, 1) \
  X(gps_rate_hz, 1, 1, 5) \
  X(gps_min_sats, 4, 3, 12) \
  X(gps_max_hdop_x100, 250, 50, 2000) \
  X(gps_stale_ms, 5000, 1000, 60000) \
  X(gps_fix_warn_ms, 60000, 1000, 300000) \
  X(sht_enabled, 1, 0, 1) \
  X(sht_ride_ms, 30000, 1000, 300000) \
  X(sht_dew_margin_mc, 2000, 0, 10000)

namespace mm {
enum class Profile : uint8_t { Bench, Field };
struct Settings {
  Profile profile = Profile::Bench;
  bool benchSleep = false;
  bool verbose = false;
  bool benchSleepLog = false; // one-shot commissioning only
#define MM_DECLARE(name, def, lo, hi) int32_t name = def;
  MM_SETTINGS_FIELDS(MM_DECLARE)
#undef MM_DECLARE
};

constexpr uint32_t SETTINGS_V1 = 0x4d4d0100UL;
constexpr uint32_t SETTINGS_V2 = 0x4d4d0200UL;
// Legacy word codec retained for non-destructive migration and fixtures.
inline uint32_t encode(const Settings& s) {
  return SETTINGS_V1 | (s.profile==Profile::Field?1u:0u)
      | (s.benchSleep?2u:0u) | (s.verbose?4u:0u) | (s.benchSleepLog?8u:0u);
}
inline bool decode(uint32_t word, Settings& s) {
  if ((word & ~uint32_t(15)) != SETTINGS_V1) return false;
  if ((word&8u) && ((word&3u)!=2u)) return false;
  Settings next;
  next.profile=(word&1)?Profile::Field:Profile::Bench;
  next.benchSleep=(word&2)!=0;
  next.verbose=(word&4)!=0;
  next.benchSleepLog=(word&8)!=0;
  s=next;
  return true;
}
inline bool settingsValid(const Settings& s) {
  if (s.profile!=Profile::Bench && s.profile!=Profile::Field) return false;
  if (s.benchSleepLog && (!s.benchSleep || s.profile!=Profile::Bench)) return false;
#define MM_RANGE(name, def, lo, hi) if (s.name < (lo) || s.name > (hi)) return false;
  MM_SETTINGS_FIELDS(MM_RANGE)
#undef MM_RANGE
  if (s.engine_start_mv <= s.engine_stop_mv ||
      s.agm_safe_mv <= s.agm_warning_mv || s.agm_warning_mv <= s.agm_panic_mv ||
      s.charge_high_mv <= s.charge_low_mv || s.long_press_ms <= s.double_click_ms ||
      s.gps_fix_warn_ms < s.gps_stale_ms) return false;
  return true;
}
#define MM_COUNT(name, def, lo, hi) + 1
constexpr size_t SETTINGS_FIELD_COUNT=0 MM_SETTINGS_FIELDS(MM_COUNT);
#undef MM_COUNT
constexpr size_t SETTINGS_BYTES=(SETTINGS_FIELD_COUNT+3)*4;
inline void putU32(uint8_t* p,uint32_t v) {
  for (unsigned i=0;i<4;++i) p[i]=uint8_t(v>>(8*i));
}
inline uint32_t getU32(const uint8_t* p) {
  uint32_t v=0;
  for (unsigned i=0;i<4;++i) v|=uint32_t(p[i])<<(8*i);
  return v;
}
inline int32_t getI32(const uint8_t* p) {
  const uint32_t u=getU32(p);
  return u<=0x7fffffffU ? int32_t(u) : int32_t(int64_t(u)-0x100000000LL);
}
inline uint32_t settingsCrc(const uint8_t* p,size_t n) {
  uint32_t crc=0xffffffffU;
  for (size_t i=0;i<n;++i) {
    crc^=p[i];
    for (unsigned b=0;b<8;++b)
      crc=(crc>>1)^((crc&1u)?0xedb88320U:0u);
  }
  return crc^0xffffffffU;
}
// Explicit LE bytes, no compiler struct padding or native bool layout.
inline bool encodeV2(const Settings& s,uint8_t* out,size_t n) {
  if (!out || n!=SETTINGS_BYTES || !settingsValid(s)) return false;
  putU32(out,SETTINGS_V2);
  putU32(out+4,encode(s));
  size_t off=8;
#define MM_WRITE(name, def, lo, hi) putU32(out+off,uint32_t(s.name)); off+=4;
  MM_SETTINGS_FIELDS(MM_WRITE)
#undef MM_WRITE
  putU32(out+off,settingsCrc(out,off));
  return true;
}
inline bool decodeV2(const uint8_t* in,size_t n,Settings& s) {
  if (!in || n!=SETTINGS_BYTES || getU32(in)!=SETTINGS_V2 ||
      getU32(in+n-4)!=settingsCrc(in,n-4)) return false;
  Settings next;
  if (!decode(getU32(in+4),next)) return false;
  size_t off=8;
#define MM_READ(name, def, lo, hi) next.name=getI32(in+off); off+=4;
  MM_SETTINGS_FIELDS(MM_READ)
#undef MM_READ
  if (!settingsValid(next)) return false;
  s=next;
  return true;
}
// Capability gates unchanged; new config fields are storage-only in V1.36.
constexpr bool FIELD_READY=false;
constexpr bool BENCH_SLEEP_READY=true;
inline bool fieldActive(const Settings& s) { return FIELD_READY && s.profile==Profile::Field; }
inline bool benchSleepActive(const Settings& s) {
  return BENCH_SLEEP_READY && s.profile==Profile::Bench && s.benchSleep;
}
inline bool verboseActive(const Settings& s) { return s.profile==Profile::Bench && s.verbose; }
}
