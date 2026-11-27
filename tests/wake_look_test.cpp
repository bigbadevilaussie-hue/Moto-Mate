#include "WakeLook.h"
#include <cassert>
#include <cstdio>
using namespace mm;
int main() {
  Gestures g;
  g.begin(0,false,false);
  assert(g.poll(30000,true,true)==Gesture::None);
  assert(g.poll(100000,false,false)==Gesture::None);
  assert(g.poll(449999,false,false)==Gesture::None);
  assert(g.poll(450000,false,false)==Gesture::Single);
  assert(g.poll(900000,false,false)==Gesture::None);
  g.begin(0,true,true);
  assert(g.poll(100000,false,false)==Gesture::None);
  assert(g.poll(449999,false,true)==Gesture::None);
  assert(g.poll(470000,true,true)==Gesture::None);
  assert(g.poll(550000,false,false)==Gesture::Double);
  assert(g.poll(1000000,false,false)==Gesture::None);
  g.begin(0,true,true);
  assert(g.poll(1499999,true,true)==Gesture::None);
  assert(g.poll(1500000,true,true)==Gesture::Long);
  assert(g.poll(9000000,true,true)==Gesture::None);
  assert(g.poll(9100000,false,false)==Gesture::None);
  g.begin(0,false,true); // EXT0 already released when CPU samples
  assert(g.poll(350000,false,false)==Gesture::Single);
  g.begin(0,true,true);
  assert(g.poll(100000,false,false)==Gesture::None);
  assert(g.poll(200000,true,true)==Gesture::None);
  assert(g.poll(1700000,true,true)==Gesture::Long);
  assert(g.poll(1750000,false,false)==Gesture::None);
  // V1.37 NVS-configured physical maintenance admission timing.
  g.configure(400000,2000000);
  g.begin(0,true,true);
  assert(g.poll(1999999,true,true)==Gesture::None);
  assert(g.poll(2000000,true,true)==Gesture::Long);
  assert(g.poll(9000000,true,true)==Gesture::None);
  assert(g.poll(9100000,false,false)==Gesture::None);
  g.begin(0,false,false);
  assert(g.poll(30000,true,true)==Gesture::None);
  assert(g.poll(100000,false,false)==Gesture::None);
  assert(g.poll(499999,false,false)==Gesture::None);
  assert(g.poll(500000,false,false)==Gesture::Single);
  WakeLook w;
  w.begin(0,WakeCause::Timer);
  w.step(1000,Gesture::None,false,true); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Button);
  w.step(1000,Gesture::None,false,true,true); assert(w.activity()==WakeActivity::Look);
  w.step(10000000,Gesture::None,false,false,true); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Button); w.step(100,Gesture::Single,false);
  assert(w.activity()==WakeActivity::RiderReady && w.deadline()==30000100);
  w.step(200,Gesture::Double,false); assert(w.deadline()==30000100);
  w.step(30000100,Gesture::None,false); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Button); w.step(100,Gesture::Double,false);
  assert(w.activity()==WakeActivity::BatteryTest && w.deadline()==10000100);
  w.step(10000100,Gesture::None,false); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Button); w.step(100,Gesture::Single,false);
  w.step(200,Gesture::None,true); assert(w.activity()==WakeActivity::Riding);
  w.step(300,Gesture::Long,false); assert(w.activity()==WakeActivity::Riding);
  w.begin(0,WakeCause::Button); w.step(100,Gesture::Long,false);
  assert(w.activity()==WakeActivity::Maintenance && w.deadline()==180000100);
  w.step(170000000,Gesture::Long,false); assert(w.deadline()==180000100);
  w.step(180000100,Gesture::Long,false); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Button); w.step(100,Gesture::Long,false);
  w.step(200,Gesture::None,false,false,false,true); assert(w.activity()==WakeActivity::SleepPending);
  w.begin(0,WakeCause::Timer); w.step(10000000,Gesture::Long,false); assert(w.activity()==WakeActivity::SleepPending);
  puts("PASS: wake/look, mutually exclusive gestures, 30/10/180s deadlines, no renewal, ride and logout transitions");
}
