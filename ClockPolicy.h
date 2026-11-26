#pragma once
#include <stdint.h>
#include <stddef.h>
namespace mm {
constexpr uint64_t UTC_MIN_US=1700000000ULL*1000000ULL;
constexpr uint64_t UTC_MAX_US=1893456000ULL*1000000ULL;
constexpr uint64_t CHECKPOINT_INTERVAL_US=3600ULL*1000000ULL;
inline bool utcInRange(uint64_t us) { return us>=UTC_MIN_US && us<=UTC_MAX_US; }
inline bool parseUtcUs(const char* s,size_t n,uint64_t& out) {
  if (!s || n!=16) return false;
  uint64_t v=0;
  for (size_t i=0;i<n;++i) {
    if (s[i]<'0'||s[i]>'9') return false;
    v=v*10+uint64_t(s[i]-'0');
  }
  if (!utcInRange(v)) return false;
  out=v; return true;
}
inline int64_t correctionUs(uint64_t before,uint64_t after) {
  return int64_t(after)-int64_t(before);
}
inline bool checkpointDue(uint64_t saved,uint64_t now) {
  if (!utcInRange(now)) return false;
  if (!utcInRange(saved)) return true;
  return now>=saved ? now-saved>=CHECKPOINT_INTERVAL_US : saved-now>=CHECKPOINT_INTERVAL_US;
}
}
