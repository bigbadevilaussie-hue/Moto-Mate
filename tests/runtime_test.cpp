#include "Runtime.h"
#include <cassert>
#include <cstdio>
int main() {
  mm::DebouncedInput b(30);
  b.begin(100,false); b.update(101,true); b.update(115,false); b.update(120,true);
  b.update(149,true); assert(!b.value() && b.edges()==0);
  b.update(150,true); assert(b.value() && b.edges()==1);
  b.update(200,false); b.update(230,false); assert(!b.value() && b.edges()==2);
  b.begin(300,true); assert(b.value() && b.edges()==0);
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
