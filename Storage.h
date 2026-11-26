#pragma once
#include <Arduino.h>
// Sole application owner of LittleFS mount and cached space statistics.
// No automatic formatting; callers must not independently mount LittleFS.
void storageBeginMaintenance();
String storageJson();
void resourcesObserveLoop(uint64_t now);
String resourcesJson();
