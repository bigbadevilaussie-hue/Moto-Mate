#pragma once
#include <stdint.h>
void otaInit();
void otaStart();
void otaStop();
void otaPoll();
bool otaRebootPending();

uint32_t otaBootId();
