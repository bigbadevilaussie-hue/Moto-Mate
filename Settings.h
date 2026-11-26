#pragma once
#include <Arduino.h>
#include "SettingsPolicy.h"
void settingsBegin();
const mm::Settings& settingsCurrent();
bool settingsHealthy();
bool settingsSave(const mm::Settings& next);
String settingsJson();
