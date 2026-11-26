#pragma once
#include <Arduino.h>
#include <FS.h>
// Sole application owner of LittleFS mount and cached space statistics.
// No automatic formatting; callers must not independently mount LittleFS.
void storageBeginMaintenance();
String storageJson();
void resourcesObserveLoop(uint64_t now);
String resourcesJson();

// BENCH-only authenticated callers in the existing maintenance server.
String storageLogIdentityJson();
bool storageProvisionLogIdentity(uint64_t generation);
bool storageAllocateTestSequence(uint64_t generation,uint64_t& seq);

// Explicit BENCH commissioning only. No automatic production log writes.
bool storageTestLog(uint64_t generation,uint32_t bootId,bool leaveOpen,String& result);
bool storageRecoverTestLog(uint64_t generation,uint64_t seq,String& result);

// Explicit read-only BENCH sealed monitor transfer; caller closes returned file.
bool storageOpenBenchTransfer(uint64_t generation,uint64_t seq,fs::File& file,String& descriptor);

// Explicit authenticated BENCH receipt. Exact fresh descriptor match required.
const char* storageBenchAck(uint64_t generation,uint64_t seq,const String& supplied);
