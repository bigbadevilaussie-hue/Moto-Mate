#pragma once
#include "LogCodec.h"
namespace mm { namespace log {
// Read-only maintenance probe. No provisioning or erase, no writes on boot.
enum class IdentityState { Unprovisioned, Valid, Invalid, IoError };
IdentityState readIdentity(Identity&);
const char* identityStateName(IdentityState);
// Explicit future storage-owner call only. Commit/readback before publishing IDs.
// No automatic retry; failure requires reconciliation. No production caller in V1.25.
bool reserveSequenceBlock(uint64_t expectedGeneration,uint64_t& first,uint64_t& last);
} }
