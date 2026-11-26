#pragma once
#include "LogCodec.h"
namespace mm { namespace log {
// Single storage owner. Calls assume mounted LittleFS and checked identity.
// No overwrite, truncate, delete, automatic formatting or radio.
enum class ScanClass { IoError, Bad, Recovered, Complete, Empty };
struct Scan { ScanClass classification; size_t bytes,records; };
const char* scanName(ScanClass);
// Pure classification used by native tests; bytes are exact on-disk image.
Scan inspectBytes(const char* path,const uint8_t* bytes,size_t size);
// Explicit commissioning operations; paths are generated internally.
bool createOpen(const Header&);
bool appendOne(const Header&,size_t expectedRecords,const uint8_t record[RECORD_BYTES]);
bool recoverOpen(Kind,uint64_t generation,uint64_t seq,Scan& result,Suffix& finalSuffix);
} }
