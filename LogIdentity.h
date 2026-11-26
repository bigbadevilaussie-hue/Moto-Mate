#pragma once
#include "LogCodec.h"
namespace mm { namespace log {
// Read-only maintenance probe. No automatic provisioning, erase or writes on boot.
enum class IdentityState { Unprovisioned, Valid, Invalid, IoError };
IdentityState readIdentity(Identity&);
const char* identityStateName(IdentityState);
// Single cooperative storage-owner calls only.
void identityBoot(bool qualifiedDeepSleep);
bool provisionVirginIdentity(uint64_t generation);
bool allocateSequence(uint64_t expectedGeneration,uint64_t& seq);
bool currentCursor(uint64_t generation,uint64_t highWater,uint64_t& next);

} }
