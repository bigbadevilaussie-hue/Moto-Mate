#include "LogIdentity.h"
#include "LogSequencePolicy.h"
#include <esp_attr.h>
#include <nvs.h>
#include <string.h>

namespace mm { namespace log {
namespace {
RTC_DATA_ATTR uint8_t retained[RETAINED_BYTES]={};
bool failed=false;
void forgetCursor() { memset(retained,0,sizeof(retained)); }
IdentityState load(nvs_handle_t h,Identity& id) {
  uint8_t bytes[ID_BYTES]; size_t n=sizeof(bytes);
  const esp_err_t e=nvs_get_blob(h,"id_v1",bytes,&n);
  if(e==ESP_ERR_NVS_NOT_FOUND) return IdentityState::Unprovisioned;
  if(e==ESP_ERR_NVS_INVALID_LENGTH || e==ESP_ERR_NVS_TYPE_MISMATCH) return IdentityState::Invalid;
  if(e!=ESP_OK) return IdentityState::IoError;
  return decodeIdentity(bytes,n,id)?IdentityState::Valid:IdentityState::Invalid;
}
}
IdentityState readIdentity(Identity& id) {
  nvs_handle_t h;
  const esp_err_t e=nvs_open("mmlog",NVS_READONLY,&h);
  if(e==ESP_ERR_NVS_NOT_FOUND) return IdentityState::Unprovisioned;
  if(e!=ESP_OK) return IdentityState::IoError;
  const IdentityState state=load(h,id);
  nvs_close(h); return state;
}
const char* identityStateName(IdentityState s) {
  switch(s) {
    case IdentityState::Unprovisioned:return "unprovisioned";
    case IdentityState::Valid:return "valid";
    case IdentityState::Invalid:return "invalid";
    default:return "io_error";
  }
}
static bool reserveSequenceBlock(uint64_t expected,uint64_t& first,uint64_t& last) {
  Identity prior={};
  if(!expected || readIdentity(prior)!=IdentityState::Valid || prior.generation!=expected) return false;
  nvs_handle_t h;
  if(nvs_open("mmlog",NVS_READWRITE,&h)!=ESP_OK) return false;
  Identity current={},next={}; uint64_t candidate=0; uint8_t bytes[ID_BYTES];
  const bool written=load(h,current)==IdentityState::Valid &&
    current.generation==expected && current.reservedThrough==prior.reservedThrough &&
    nextReservation(current,next,candidate) && encodeIdentity(next,bytes) &&
    nvs_set_blob(h,"id_v1",bytes,sizeof(bytes))==ESP_OK && nvs_commit(h)==ESP_OK;
  nvs_close(h);
  Identity verified={};
  if(!written || readIdentity(verified)!=IdentityState::Valid ||
     verified.generation!=next.generation || verified.reservedThrough!=next.reservedThrough) return false;
  first=candidate; last=next.reservedThrough; return true;
}
void identityBoot(bool qualifiedDeepSleep) {
  failed=false;
  Cursor c={};
  if(!qualifiedDeepSleep || !decodeCursor(retained,sizeof(retained),c)) forgetCursor();
}
bool provisionVirginIdentity(uint64_t generation) {
  if(failed || !generation) return false;
  // Even an existing empty namespace is suspicious. Do not recover identity
  // automatically if the key or trip checkpoint has disappeared.
  nvs_handle_t h;
  const esp_err_t prior=nvs_open("mmlog",NVS_READONLY,&h);
  if(prior==ESP_OK) { nvs_close(h); return false; }
  if(prior!=ESP_ERR_NVS_NOT_FOUND) return false;
  uint8_t raw[ID_BYTES]; const Identity fresh={generation,0};
  if(!encodeIdentity(fresh,raw)) return false;
  if(nvs_open("mmlog",NVS_READWRITE,&h)!=ESP_OK) { failed=true; return false; }
  const bool ok=nvs_set_blob(h,"id_v1",raw,sizeof(raw))==ESP_OK && nvs_commit(h)==ESP_OK;
  nvs_close(h);
  Identity verified={};
  if(!ok || readIdentity(verified)!=IdentityState::Valid ||
     verified.generation!=generation || verified.reservedThrough!=0) {
    failed=true; forgetCursor(); return false;
  }
  forgetCursor(); return true;
}
bool currentCursor(uint64_t generation,uint64_t highWater,uint64_t& next) {
  Cursor c={}; Identity id={generation,highWater};
  if(failed || !decodeCursor(retained,sizeof(retained),c) || !cursorMatches(c,id)) return false;
  next=c.next; return true;
}
bool allocateSequence(uint64_t expected,uint64_t& seq) {
  if(failed || !expected) return false;
  Identity id={};
  if(readIdentity(id)!=IdentityState::Valid || id.generation!=expected) {
    forgetCursor(); return false;
  }
  Cursor c={};
  if(!decodeCursor(retained,sizeof(retained),c) || !cursorMatches(c,id) || c.next>c.last) {
    forgetCursor();
    uint64_t first=0,last=0;
    if(!reserveSequenceBlock(expected,first,last)) { failed=true; return false; }
    c={expected,first,last};
  }
  uint64_t candidate=0;
  // Advance and encode before exposing ID. A reset during this operation
  // either discards the cursor or leaves an invalid CRC; reserved IDs are skipped.
  if(!takeSequence(c,candidate) || !encodeCursor(c,retained)) {
    failed=true; forgetCursor(); return false;
  }
  seq=candidate; return true;
}
} }
