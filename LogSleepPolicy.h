#pragma once
#include <stdint.h>
namespace mm { namespace log {
constexpr uint32_t SLEEP_LOG_CYCLES=3;
constexpr uint64_t SLEEP_SAMPLE_DUE_US=3000000ULL;
constexpr uint64_t SLEEP_SAMPLE_LATEST_US=10000000ULL;
inline bool sleepLogDue(bool enabled,bool attempted,uint32_t cycle,uint64_t now) {
  return enabled && !attempted && cycle>=1 && cycle<=SLEEP_LOG_CYCLES &&
    now>=SLEEP_SAMPLE_DUE_US && now<=SLEEP_SAMPLE_LATEST_US;
}
inline bool sleepLogContinue(bool enabled,bool success,uint32_t cycle) {
  return enabled && success && cycle>=1 && cycle<SLEEP_LOG_CYCLES;
}
} }
