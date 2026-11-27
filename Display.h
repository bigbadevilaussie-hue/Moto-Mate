#pragma once
#include "Splash.h"
void displayBegin(bool animate=true);
bool displaySleepCountdown(unsigned seconds);
void displaySleepPanel();
void displayResumePanel();
void displayPoll(SplashMode mode);
void displayBenchLive(unsigned seconds,float voltage,float temperature,float humidity);

// Presentation only: absolute deadlines are owned by the coordinator/OTA service.
enum class UploadView : uint8_t { Receiving, Verifying, Committed, Failed };
void displayMaintenance(bool connected,uint64_t deadlineUs);
void displayFirmwareUpload(UploadView stage,size_t accepted,size_t total,uint64_t deadlineUs);
