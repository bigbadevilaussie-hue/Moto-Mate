#pragma once
#include <stdint.h>
// One-use, successful-OTA-only continuation of the physical maintenance lease.
void otaLeaseArm(uint64_t deadlineUs,uint32_t targetAddress);
uint64_t otaLeaseConsume();
