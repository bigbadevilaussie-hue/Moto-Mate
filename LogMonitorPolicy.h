#pragma once
#include <stdint.h>
namespace mm { namespace log {
constexpr uint64_t BENCH_FIRST_US=3000000ULL;
constexpr uint64_t MONITOR_PERIOD_US=300000000ULL;
constexpr unsigned BENCH_MAX_SAMPLES=4;
inline bool benchCountOK(unsigned count) { return count>=1 && count<=BENCH_MAX_SAMPLES; }
// No catch-up burst: next deadline is relative to actual acquisition.
inline uint64_t nextMonitorDue(uint64_t now,bool first) {
  return now+(first?BENCH_FIRST_US:MONITOR_PERIOD_US);
}
} }
