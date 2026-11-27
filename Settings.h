#pragma once
#include <Arduino.h>
#include "SettingsPolicy.h"
void settingsBegin();
const mm::Settings& settingsCurrent();
bool settingsHealthy();
bool settingsSave(const mm::Settings& next);
String settingsJson();

// Independent BENCH simulator setting, preserves existing schema-2 ABI.
bool settingsFakeAdcGet(uint16_t& mv);
bool settingsFakeAdcSet(uint16_t mv);
