#include "../LogMonitorPolicy.h"
#include <assert.h>
#include <stdio.h>
int main() {
  using namespace mm::log;
  assert(!benchCountOK(0) && benchCountOK(1) && benchCountOK(4) && !benchCountOK(5));
  assert(nextMonitorDue(100,true)==3000100);
  assert(nextMonitorDue(900000000,false)==1200000000);
  puts("BENCH monitor bounded count and cadence PASS");
}
