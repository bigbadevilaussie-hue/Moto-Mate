#pragma once
#include "Runtime.h"
namespace mm {
// Provisional gesture timings; qualify on the actual switch and EXT0 boot.
constexpr TimeUs DOUBLE_TAP_US=350000ULL;
constexpr TimeUs LONG_PRESS_US=1500000ULL;
constexpr TimeUs RIDER_READY_US=30000000ULL;
constexpr TimeUs BATTERY_TEST_US=10000000ULL;
constexpr TimeUs MAINTENANCE_US=180000000ULL;
enum class Gesture : uint8_t { None, Single, Double, Long };
enum class WakeCause : uint8_t { Cold, Timer, Button, Other };
enum class WakeActivity : uint8_t { Look, RiderReady, BatteryTest, Maintenance, Riding, SleepPending };

// Feed the existing stable and raw GPIO samples, never a second debounce owner.
class Gestures {
public:
  void configure(TimeUs doubleTap, TimeUs longPress) { doubleTap_=doubleTap; longPress_=longPress; }
  void begin(TimeUs now, bool stable, bool buttonWake) {
    down_=stable; started_=now; released_=now; emitted_=false;
    waiting_=buttonWake && !stable; second_=false;
  }
  Gesture poll(TimeUs now, bool stable, bool raw) {
    if (stable!=down_) {
      down_=stable;
      if (stable) {
        // A raw second press may have begun just before the deadline. If
        // observed while pending, poll deferred expiry until it debounced.
        if (waiting_) second_=true;
        waiting_=false; started_=now; emitted_=false;
      } else {
        if (emitted_) { second_=false; return Gesture::None; }
        if (now-started_>=longPress_) { second_=false; return Gesture::Long; }
        if (second_) { second_=false; return Gesture::Double; }
        waiting_=true; released_=now;
      }
    }
    if (down_ && raw && !emitted_ && now-started_>=longPress_) {
      emitted_=true; waiting_=false; second_=false; return Gesture::Long;
    }
    if (waiting_ && !raw && now-released_>=doubleTap_) {
      waiting_=false; return Gesture::Single;
    }
    return Gesture::None;
  }
  bool pending() const { return down_ || waiting_; }
private:
  bool down_=false, waiting_=false, second_=false, emitted_=false;
  TimeUs started_=0, released_=0;
  TimeUs doubleTap_=DOUBLE_TAP_US, longPress_=LONG_PRESS_US;
};

// Decision policy only: caller owns qualified sensors, radios, storage and sleep.
// No command, poll, gesture or reconnect can extend an accepted maintenance lease.
class WakeLook {
public:
  void begin(TimeUs now, WakeCause cause) {
    cause_=cause; activity_=WakeActivity::Look;
    deadline_=now+PARKED_AWAKE_LIMIT_US;
  }
  void step(TimeUs now, Gesture gesture, bool qualifiedRunning,
            bool lookComplete=false, bool inputPending=false, bool logout=false) {
    if (activity_==WakeActivity::SleepPending) return;
    if (activity_==WakeActivity::Maintenance) {
      if (logout || now>=deadline_) activity_=qualifiedRunning?WakeActivity::Riding:WakeActivity::SleepPending;
      return;
    }
    if (qualifiedRunning) { activity_=WakeActivity::Riding; return; }
    if (activity_==WakeActivity::Riding) return; // ride-stop qualification separate
    if (now>=deadline_) { activity_=WakeActivity::SleepPending; return; }
    if (activity_!=WakeActivity::Look) return; // timed screens cannot be extended
    switch (gesture) {
      case Gesture::Single: activity_=WakeActivity::RiderReady; deadline_=now+RIDER_READY_US; return;
      case Gesture::Double: activity_=WakeActivity::BatteryTest; deadline_=now+BATTERY_TEST_US; return;
      case Gesture::Long: activity_=WakeActivity::Maintenance; deadline_=now+MAINTENANCE_US; return;
      default: break;
    }
    if (lookComplete && !inputPending) activity_=WakeActivity::SleepPending;
  }
  WakeActivity activity() const { return activity_; }
  WakeCause cause() const { return cause_; }
  TimeUs deadline() const { return deadline_; }
private:
  WakeCause cause_=WakeCause::Other;
  WakeActivity activity_=WakeActivity::SleepPending;
  TimeUs deadline_=0;
};
} // namespace mm
