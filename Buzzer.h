#pragma once
#include <Arduino.h>

extern bool alarmSequenceActive;
extern bool lowBattBeepActive;
extern int lowBattBeepRemaining;
extern unsigned long lowBattBeepTimer;

void buzzerOn();
void buzzerOff();
void updateBeeps(unsigned long now);
