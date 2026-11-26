#pragma once

// BENCH association service: poll returns true once connected or attempts exhausted.
void wifiBegin();
bool wifiPoll();
bool wifiConnected();
