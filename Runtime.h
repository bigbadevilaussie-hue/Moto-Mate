#pragma once
#include <stdint.h>

namespace mm {
// All times are monotonic microseconds since this boot, never wall-clock time.
using TimeUs = uint64_t;

// Fixed parked policy; no wall clock or NVS interval override.
constexpr TimeUs PARKED_SLEEP_US = 300000000ULL;
constexpr TimeUs PARKED_AWAKE_LIMIT_US = 10000000ULL;
constexpr TimeUs COLLECTION_INTERVAL_US = 86400000000ULL;
static_assert(PARKED_SLEEP_US == 300ULL*1000000ULL, "fixed sleep policy");
static_assert(PARKED_AWAKE_LIMIT_US == 10ULL*1000000ULL, "fixed awake limit");

class Periodic {
public:
  explicit Periodic(TimeUs intervalUs) : interval_(intervalUs) {}
  void start(TimeUs now) { last_ = now; }
  bool due(TimeUs now) {
    if (now - last_ < interval_) return false;
    last_ = now;  // At most one dispatch per check; no catch-up bursts.
    return true;
  }
private:
  const TimeUs interval_;
  TimeUs last_ = 0;
};

// Input sampling is independent of gesture/action policy; no delays.
class DebouncedInput {
public:
  explicit DebouncedInput(TimeUs debounceUs): debounce_(debounceUs) {}
  void begin(TimeUs now,bool pressed) {
    stable_=candidate_=bootPressed_=pressed; changed_=stableSince_=now;
    edges_=0; lastHold_=0;
  }
  void update(TimeUs now,bool pressed) {
    if (pressed!=candidate_) { candidate_=pressed; changed_=now; }
    if (candidate_!=stable_ && now-changed_>=debounce_) {
      if (stable_) lastHold_=now-stableSince_;
      stable_=candidate_; stableSince_=now; ++edges_;
    }
  }
  bool value() const { return stable_; }
  bool rawValue() const { return candidate_; }
  bool bootPressed() const { return bootPressed_; }
  uint32_t edges() const { return edges_; }
  TimeUs heldUs(TimeUs now) const { return stable_?now-stableSince_:0; }
  TimeUs lastHoldUs() const { return lastHold_; }
  bool releasedForWake(TimeUs now) const {
    return !stable_ && !candidate_ && now-changed_>=debounce_;
  }
private:
  const TimeUs debounce_;
  TimeUs changed_=0,stableSince_=0,lastHold_=0;
  bool stable_=false,candidate_=false,bootPressed_=false;
  uint32_t edges_=0;
};

struct LoopMetrics {
  TimeUs maxGapUs = 0;
  TimeUs maxServiceUs = 0;
  uint32_t overruns = 0;
  void recordService(TimeUs duration, TimeUs budget) {
    if (duration > maxServiceUs) maxServiceUs = duration;
    if (duration > budget) ++overruns;
  }
};

enum class State : uint8_t { Boot, Monitoring, SleepPending, BenchIdle, Fault };

class Coordinator {
public:
  explicit Coordinator(TimeUs wakeBudgetUs) : wakeBudget_(wakeBudgetUs) {}
  void begin(TimeUs now) {
    state_ = State::Boot;
    started_ = lastLoop_ = now;
    metrics_ = LoopMetrics{};
  }
  void tick(TimeUs now) {
    const TimeUs gap = now - lastLoop_;
    if (gap > metrics_.maxGapUs) metrics_.maxGapUs = gap;
    lastLoop_ = now;
    checkBudget(now);
  }
  void finishBoot(TimeUs now, bool passed) {
    if (state_ != State::Boot) return;
    state_ = passed && now - started_ < wakeBudget_ ? State::Monitoring : State::Fault;
  }
  bool acknowledgeBenchSleep() {
    if (state_ != State::SleepPending) return false;
    state_ = State::BenchIdle;
    return true;
  }
  State state() const { return state_; }
  TimeUs awakeUs(TimeUs now) const { return now - started_; }
  const LoopMetrics& metrics() const { return metrics_; }
  void recordService(TimeUs duration, TimeUs budget) { metrics_.recordService(duration, budget); }
private:
  void checkBudget(TimeUs now) {
    if (awakeUs(now) < wakeBudget_) return;
    if (state_ == State::Boot) state_ = State::Fault;
    else if (state_ == State::Monitoring) state_ = State::SleepPending;
  }
  const TimeUs wakeBudget_;
  State state_ = State::Boot;
  TimeUs started_ = 0;
  TimeUs lastLoop_ = 0;
  LoopMetrics metrics_;
};
} // namespace mm
