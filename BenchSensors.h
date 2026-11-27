#pragma once
#include <Arduino.h>
#include "LogCodec.h"
void sensorsBegin(bool deepSleepWake);
void sensorsPoll(uint64_t now);
bool sensorsQuiesce();
void sensorsResume();
String sensorsJson();
void sensorsBenchEnvironment(uint64_t now,float& temperature,float& humidity);

// Fresh cached measurements only; caller zero-initializes payload and validity.
void sensorsLogSample(uint64_t now,mm::log::Sample& sample,uint16_t& validity);
