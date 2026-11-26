#pragma once
#include <Arduino.h>
void sensorsBegin(bool deepSleepWake);
void sensorsPoll(uint64_t now);
bool sensorsQuiesce();
void sensorsResume();
String sensorsJson();
