#pragma once
#include "LogCodec.h"
#include <string.h>
namespace mm { namespace log {
constexpr unsigned INVENTORY_MAX_ENTRIES=256;
constexpr unsigned RECOVERY_MAX_FILES=16;
inline bool parseMonitorName(const char* name,uint64_t& generation,uint64_t& seq,Suffix& suffix) {
  if(!name || strlen(name)<39 || strncmp(name,"m1_",3)) return false;
  uint64_t numbers[2]={};
  for(unsigned i=0;i<32;++i) {
    const char c=name[3+i];
    if(c>='0' && c<='9') numbers[i/16]=(numbers[i/16]<<4)|uint64_t(c-'0');
    else if(c>='a' && c<='f') numbers[i/16]=(numbers[i/16]<<4)|uint64_t(c-'a'+10);
    else return false;
  }
  if(!numbers[0] || !numbers[1]) return false;
  const char* tail=name+35;
  const char* endings[]={".open",".bin",".bin.gz",".bin.gz.tmp",".rec",".bad"};
  for(unsigned i=0;i<6;++i) {
    if(!strcmp(tail,endings[i])) {
      generation=numbers[0]; seq=numbers[1]; suffix=static_cast<Suffix>(i); return true;
    }
  }
  return false;
}
} }
