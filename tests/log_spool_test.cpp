#include "../LogSpool.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace mm::log;
int main() {
  Header h={1,1,{2,0,0,0,0,1},1,27,123,1,0,0,0,0,300000,1,0};
  uint8_t bytes[HEADER_BYTES+3*RECORD_BYTES]={};
  char path[PATH_BYTES];
  assert(filename(Kind::Monitor,123,1,Suffix::Open,path));
  assert(encodeHeader(h,bytes));
  Prefix p={0,1,0,7,0x40,0};
  Boot b={1,0,UINT32_MAX,UINT32_MAX,0,0};
  for(unsigned i=0;i<3;++i) assert(encode(p,b,bytes+HEADER_BYTES+i*RECORD_BYTES));
  assert(inspectBytes(path,bytes,HEADER_BYTES).classification==ScanClass::Empty);
  assert(inspectBytes(path,bytes,HEADER_BYTES-1).classification==ScanClass::Bad);
  Scan s=inspectBytes(path,bytes,sizeof(bytes));
  assert(s.classification==ScanClass::Complete && s.records==3);
  s=inspectBytes(path,bytes,sizeof(bytes)-1);
  assert(s.classification==ScanClass::Recovered && s.records==2);
  bytes[HEADER_BYTES+RECORD_BYTES+20]^=1;
  s=inspectBytes(path,bytes,sizeof(bytes));
  assert(s.classification==ScanClass::Recovered && s.records==1);
  bytes[HEADER_BYTES+20]^=1;
  assert(inspectBytes(path,bytes,sizeof(bytes)).classification==ScanClass::Bad);
  bytes[0]^=1;
  assert(inspectBytes(path,bytes,sizeof(bytes)).classification==ScanClass::Bad);
  puts("spool contiguous-prefix and recovery fixtures PASS");
}
