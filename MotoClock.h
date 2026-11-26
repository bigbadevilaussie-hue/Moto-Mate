#pragma once
#include <Arduino.h>
// Called only by the main loop/setup and authenticated HTTP callbacks.
// Future BLE callbacks must queue input to the same owner.
void clockBegin(bool deepSleepWake);
uint64_t clockUtcUs(); // zero when UTC provenance is unknown
bool clockApplyHostUtc(uint64_t utcUs);
String clockJson();

// Sets only clock quality bits 0..1; returns zero when unknown.
uint64_t clockLogUtcMs(uint16_t& validity);
