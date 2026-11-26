#pragma once
#include <stdint.h>
#include <stddef.h>

// Locked schema 1. In-memory structs are never written to flash.
// All encode functions require a correctly sized output buffer.
// False means the output must not be written or used.
namespace mm { namespace log {
constexpr size_t HEADER_BYTES=80, RECORD_BYTES=48, ID_BYTES=24, PATH_BYTES=48;
constexpr size_t MAX_RAW_BYTES=16384, MAX_RECORDS=339;
constexpr uint64_t BLOCK_SIZE=32;
enum class Kind : uint8_t { Monitor=1, Ride=2 };
enum class Suffix : uint8_t { Open, Bin, Gzip, GzipTemp, Recovered, Bad };
enum class Type : uint8_t {
  SAMPLE=1,
  CRANK_SAMPLE=2,
  TRIP_START=3,
  LEG_START=4,
  LEG_END=5,
  TRIP_END=6,
  BOOT=7,
  CLOCK_SYNC=8,
  OPERATION=9,
  TRANSFER=10,
  GAP=11,
  WAKE_SUMMARY=12
};
uint32_t crc32(const uint8_t* data,size_t n);
uint64_t get(const uint8_t* data,size_t n);
struct Header {
  uint8_t kind;
  uint16_t flags;
  uint8_t device_mac[6];
  uint8_t fw_major;
  uint8_t fw_minor;
  uint64_t generation;
  uint64_t file_seq;
  uint64_t trip_seq;
  uint32_t leg;
  uint32_t part;
  uint64_t opened_utc_ms;
  uint32_t nominal_period_ms;
  uint32_t opened_boot_id;
  uint32_t opened_mono_ms;
};
struct Prefix {
  uint64_t utc_ms;
  uint32_t boot_id;
  uint32_t mono_ms;
  uint8_t type;
  uint8_t state;
  uint16_t validity;
};
struct Sample {
  uint16_t battery_mv;
  int16_t temperature_centi_c;
  uint16_t humidity_centi_pct;
  int32_t lat_e7;
  int32_t lon_e7;
  uint16_t speed_dkmh;
  int16_t altitude_m;
  uint16_t course_ddeg;
  uint8_t satellites;
  uint8_t sample_flags;
  uint16_t gravity_delta_mg;
};
struct Lifecycle {
  uint64_t trip_seq;
  uint32_t leg;
  uint16_t reason;
  uint16_t flags;
  uint64_t duration_ms;
};
struct Boot {
  uint16_t reset_reason;
  uint16_t wake_cause;
  uint32_t prior_awake_us;
  uint32_t prior_sleep_ms;
  uint32_t lost_total;
  uint32_t flags;
};
struct ClockSync {
  uint64_t prior_utc_ms;
  uint64_t new_utc_ms;
  int64_t correction_ms;
};
struct Operation {
  uint16_t code;
  uint16_t result;
  uint16_t battery_mv;
  int16_t temperature_centi_c;
  uint32_t detail_code;
  uint32_t arg0;
  uint32_t arg1;
};
struct Transfer {
  uint64_t file_seq;
  uint32_t bytes;
  uint32_t duration_ms;
  uint16_t result;
  uint16_t attempts;
};
struct Gap {
  uint32_t monitor_lost;
  uint32_t ride_lost;
  uint32_t crank_lost;
  uint32_t event_lost;
  uint32_t duration_ms;
  uint16_t reason;
};
struct WakeSummary {
  uint32_t awake_us;
  uint32_t planned_sleep_ms;
  uint32_t completed_mask;
  uint32_t deferred_mask;
  uint32_t min_heap_bytes;
  uint32_t max_loop_gap_us;
};
bool encodeHeader(const Header&,uint8_t out[HEADER_BYTES]);
bool validHeader(const uint8_t*,size_t);
bool validRecord(const uint8_t*,size_t);
bool recordMatchesHeader(const uint8_t* header,size_t headerLen,const uint8_t* record,size_t recordLen);
bool encode(const Prefix&,const Sample&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const Lifecycle&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const Boot&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const ClockSync&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const Operation&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const Transfer&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const Gap&,uint8_t out[RECORD_BYTES]);
bool encode(const Prefix&,const WakeSummary&,uint8_t out[RECORD_BYTES]);
bool filename(Kind,uint64_t generation,uint64_t seq,Suffix,char out[PATH_BYTES]);
bool filenameMatchesHeader(const char* path,const uint8_t*,size_t);
struct Identity { uint64_t generation; uint64_t reservedThrough; };
bool encodeIdentity(const Identity&,uint8_t out[ID_BYTES]);
bool decodeIdentity(const uint8_t*,size_t,Identity&);
bool nextReservation(const Identity&,Identity& next,uint64_t& first);
} }
