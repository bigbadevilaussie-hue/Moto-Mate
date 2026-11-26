#pragma once
#include <stdint.h>
namespace mm {
enum class Profile : uint8_t { Bench, Field };
struct Settings {
  Profile profile = Profile::Bench;
  bool benchSleep = false;
  bool verbose = false;
};
constexpr uint32_t SETTINGS_V1 = 0x4d4d0100UL;
// One NVS value: schema and all flags committed together.
inline uint32_t encode(const Settings& s) {
  return SETTINGS_V1 | (s.profile==Profile::Field?1u:0u)
      | (s.benchSleep?2u:0u) | (s.verbose?4u:0u);
}
inline bool decode(uint32_t word, Settings& s) {
  if ((word & ~uint32_t(7)) != SETTINGS_V1) return false;
  s.profile=(word&1)?Profile::Field:Profile::Bench;
  s.benchSleep=(word&2)!=0;
  s.verbose=(word&4)!=0;
  return true;
}
// Field must never inherit bench overrides. Capability gates are false
// until the relevant runtime and recovery paths have been implemented.
constexpr bool FIELD_READY=false;
// One-shot commissioning on next reset; request consumed before sleep.
constexpr bool BENCH_SLEEP_READY=true;
inline bool fieldActive(const Settings& s) { return FIELD_READY && s.profile==Profile::Field; }
inline bool benchSleepActive(const Settings& s) {
  return BENCH_SLEEP_READY && s.profile==Profile::Bench && s.benchSleep;
}
inline bool verboseActive(const Settings& s) { return s.profile==Profile::Bench && s.verbose; }
}
