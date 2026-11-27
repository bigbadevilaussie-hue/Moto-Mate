#pragma once
#include <stdint.h>
// Normal BENCH only. Loop owns start, snapshot updates and stop.
bool benchBleBegin(uint32_t bootId);
void benchBlePoll(uint64_t now,uint32_t cycle,uint32_t cause,uint64_t priorUs);
bool benchBleStop();
