#include "../LogSleepPolicy.h"
#include <assert.h>
#include <stdio.h>
int main() {
  using namespace mm::log;
  assert(!sleepLogDue(false,false,1,3000000));
  assert(!sleepLogDue(true,true,1,3000000));
  assert(!sleepLogDue(true,false,1,2999999));
  assert(sleepLogDue(true,false,1,3000000));
  assert(sleepLogDue(true,false,3,10000000));
  assert(!sleepLogDue(true,false,3,10000001));
  assert(!sleepLogDue(true,false,4,3000000));
  assert(sleepLogContinue(true,true,1) && sleepLogContinue(true,true,2));
  assert(!sleepLogContinue(true,true,3) && !sleepLogContinue(true,false,1));
  puts("PASS: opt-in bounded sleep logging policy");
}
