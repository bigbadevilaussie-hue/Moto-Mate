#pragma once
#include <Arduino.h>
// Shared Wire bus: initialize once in BENCH; no background tasks or wake source.
void rtc8563Begin();
String rtc8563Json(uint64_t trustedSystemUtcUs);
void rtc8563Sync(uint64_t trustedSystemUtcUs);
