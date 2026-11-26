#include "ClockPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  uint64_t t=0;
  assert(mm::parseUtcUs("1795652966123456",16,t) && t==1795652966123456ULL);
  assert(mm::utcInRange(mm::UTC_MIN_US) && mm::utcInRange(mm::UTC_MAX_US));
  assert(!mm::utcInRange(mm::UTC_MIN_US-1) && !mm::utcInRange(mm::UTC_MAX_US+1));
  assert(!mm::parseUtcUs("179565296612345x",16,t));
  assert(!mm::parseUtcUs("1795652966123456",15,t));
  assert(!mm::parseUtcUs("2147483648000000",16,t));
  assert(!mm::parseUtcUs("0000000000000000",16,t));
  assert(mm::correctionUs(t,t+5000000)==5000000);
  assert(mm::correctionUs(t,t-5000000)==-5000000);
  assert(mm::checkpointDue(0,t));
  assert(!mm::checkpointDue(t,t+3599999999ULL));
  assert(mm::checkpointDue(t,t+3600000000ULL));
  assert(mm::checkpointDue(t,t-3600000000ULL));
  puts("PASS: UTC validation, signed corrections, checkpoint policy");
}
