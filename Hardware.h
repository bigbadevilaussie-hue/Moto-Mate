#pragma once
#include <Arduino.h>
#include "Runtime.h"
#include "WakeLook.h"
// Only this module establishes safe idle output levels during commissioning.
// No load activation, display init or battery protection until wiring is qualified.
void hardwareBegin(mm::TimeUs now);
void hardwareReportBoot();
// Active-high control input on BENCH TFT module; caller owns display policy.
bool hardwareBacklight(bool on);
void hardwarePoll(mm::TimeUs now);
void hardwareGestureTiming(mm::TimeUs doubleTap, mm::TimeUs longPress);
float hardwareBenchVoltage(mm::TimeUs now);
mm::Gesture hardwareTakeGesture();
bool hardwareGesturePending();
String hardwareStatus();

// Caller must verify radios are off before arming sleep.
bool hardwareButtonReleased(mm::TimeUs now);
bool hardwareArmBenchSleep(mm::TimeUs sleepUs, bool buttonWake, bool keepBacklight=false);
void hardwareCancelBenchSleep();
