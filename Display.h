#pragma once
#include "Splash.h"
void displayBegin(bool animate=true);
bool displaySleepCountdown(unsigned seconds);
void displaySleepPanel();
void displayResumePanel();
void displayPoll(SplashMode mode);
void displayBenchLive(unsigned seconds,float voltage,float temperature,float humidity);
