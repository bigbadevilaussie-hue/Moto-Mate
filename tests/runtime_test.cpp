#include "Runtime.h"
#include <cassert>
#include <cstdio>
int main() {
  assert(mm::PARKED_SLEEP_US==300000000ULL);
  assert(mm::PARKED_AWAKE_LIMIT_US==10000000ULL);
  assert(mm::COLLECTION_INTERVAL_US==86400000000ULL);
  mm::DebouncedInput b(30);
  b.begin(100,false); b.update(101,true); b.update(115,false); b.update(120,true);
  b.update(149,true); assert(!b.value() && b.edges()==0);
  b.update(150,true); assert(b.value() && b.edges()==1);
  b.update(200,false); b.update(230,false); assert(!b.value() && b.edges()==2);
  b.begin(300,true); assert(b.value() && b.edges()==0);
  mm::DebouncedInput wake(30000);
  wake.begin(100,false);
  assert(!wake.releasedForWake(30099)); assert(wake.releasedForWake(30100));
  wake.update(31000,true); assert(!wake.releasedForWake(31000));
  wake.update(32000,false); wake.update(33000,true);
  wake.update(62999,true); assert(!wake.value() && wake.edges()==0);
  wake.update(63000,true); assert(wake.value() && wake.edges()==1);
  assert(wake.heldUs(163000)==100000);
  wake.update(200000,false); assert(!wake.releasedForWake(200000));
  wake.update(230000,false);
  assert(wake.edges()==2 && wake.lastHoldUs()==167000 && wake.releasedForWake(230000));
  wake.begin(300000,true); assert(wake.bootPressed() && !wake.releasedForWake(999999));
  wake.update(1000000,false); wake.update(1030000,false);
  assert(wake.edges()==1 && wake.lastHoldUs()==730000 && wake.releasedForWake(1030000));
  wake.update(1040000,true); wake.update(1041000,false);
  assert(!wake.releasedForWake(1070999));
  assert(wake.releasedForWake(1071000) && wake.edges()==1);
  mm::Periodic p(10);
  p.start(100);
  assert(!p.due(109)); assert(p.due(110));
  assert(p.due(150)); assert(!p.due(150)); assert(!p.due(159)); assert(p.due(160));
  mm::Coordinator c(100);
  c.begin(1000);
  assert(c.state() == mm::State::Boot); assert(!c.acknowledgeBenchSleep());
  c.tick(1010); c.finishBoot(1010, true);
  assert(c.state() == mm::State::Monitoring);
  c.tick(1099); assert(c.state() == mm::State::Monitoring);
  c.recordService(8, 10); c.recordService(12, 10);
  assert(c.metrics().maxGapUs == 89 && c.metrics().maxServiceUs == 12 && c.metrics().overruns == 1);
  c.tick(1100); assert(c.state() == mm::State::SleepPending);
  assert(c.acknowledgeBenchSleep()); assert(!c.acknowledgeBenchSleep());
  c.tick(1200); assert(c.state() == mm::State::BenchIdle);
  c.begin(2000); assert(c.metrics().maxGapUs == 0 && c.metrics().overruns == 0);
  c.tick(2100); assert(c.state() == mm::State::Fault);
  c.finishBoot(2101,true); assert(c.state() == mm::State::Fault);
  c.begin(3000); c.finishBoot(3001,false); assert(c.state() == mm::State::Fault);
  c.begin(4000); c.finishBoot(4100,true); assert(c.state() == mm::State::Fault);
  puts("PASS: periodic scheduling, state transitions, deadlines, metrics, failure paths");
}
