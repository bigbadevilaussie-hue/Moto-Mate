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

// Volatile, bounded maintenance-only sampling session; never armed by boot.
bool storageMonitorActive();
String storageMonitorJson();
bool storageMonitorStart(uint64_t generation,uint32_t bootId,unsigned count);
bool storageMonitorSeal();
void storageMonitorCancel(const char* reason);
void storageMonitorPoll(uint64_t now);

// Explicit radio-free sleep logging commissioning, one sample per selected wake.
bool storageSleepLogReady(uint64_t generation);
bool storageSleepSample(uint32_t bootId,uint32_t cycle);

void storageSleepForget();

// Only called after entering existing BENCH maintenance, never on normal sleep wakes.
void storageRecoverMaintenance();
bool storageInventory(String& result);

// Default autonomous BENCH monitor; separate from explicit legacy commissioning.
void storageAutoBegin(bool enabled,bool retainedWake);
void storageAutoPoll(uint64_t now,uint32_t bootId,bool maintenance);
