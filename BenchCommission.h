#pragma once
#include <stdint.h>

// USB-only commissioning; never a FIELD wake path.
void commissionBanner(int wakeCause);
bool commissionStorage();
// Start / poll / cancel a bounded, Wi-Fi-only test. BLE not yet ported.
void commissionRadioBegin(bool keepWifi = false);
bool commissionWifiConnected();
bool commissionRadioPoll(); // true when complete
bool commissionRadioPassed();
void commissionRadioCancel();
