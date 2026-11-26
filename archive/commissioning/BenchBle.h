#pragma once
// Isolated USB bench protocol B1. Never used for field operation.
void commissionBleBegin();
bool commissionBlePoll();
bool commissionBlePassed();
void commissionBleCancel();
