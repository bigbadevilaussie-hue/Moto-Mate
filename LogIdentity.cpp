#include "LogIdentity.h"
#include <nvs.h>
#include <string.h>

namespace mm { namespace log {
namespace {
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
bool reserveSequenceBlock(uint64_t expected,uint64_t& first,uint64_t& last) {
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
} }
