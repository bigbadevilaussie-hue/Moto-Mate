#pragma once
#include <stdint.h>
void otaInit();
void otaStart(uint64_t deadlineUs);
void otaStop();
void otaPoll();
bool otaRebootPending();
bool otaBusy();

uint32_t otaBootId();
