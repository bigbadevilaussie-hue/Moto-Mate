#pragma once
#include <Arduino.h>
#include "Runtime.h"
#include "WakeLook.h"
// Only this module establishes safe idle output levels during commissioning.
// No load activation, display init or battery protection until wiring is qualified.
void hardwareBegin(mm::TimeUs now);
void hardwareReportBoot();
void hardwarePoll(mm::TimeUs now);
mm::Gesture hardwareTakeGesture();
bool hardwareGesturePending();
String hardwareStatus();

// Explicit bench test only; caller must have never started the radios.
bool hardwareButtonReleased(mm::TimeUs now);
bool hardwareArmBenchSleep();
void hardwareCancelBenchSleep();
