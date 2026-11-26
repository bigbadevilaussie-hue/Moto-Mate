#include "LogSpool.h"
namespace mm { namespace log {
const char* scanName(ScanClass c) {
  switch(c) {
    case ScanClass::Bad:return "bad";
    case ScanClass::Recovered:return "recovered";
    case ScanClass::Complete:return "complete";
    case ScanClass::Empty:return "empty";
    default:return "io_error";
  }
}
Scan inspectBytes(const char* path,const uint8_t* bytes,size_t size) {
  Scan s={ScanClass::Bad,size,0};
  if(!bytes || size<HEADER_BYTES || !filenameMatchesHeader(path,bytes,HEADER_BYTES)) return s;
  while(s.records<MAX_RECORDS && size-HEADER_BYTES-s.records*RECORD_BYTES>=RECORD_BYTES) {
    if(!recordMatchesHeader(bytes,HEADER_BYTES,bytes+HEADER_BYTES+s.records*RECORD_BYTES,RECORD_BYTES)) break;
    ++s.records;
  }
  if(!s.records) { if(size==HEADER_BYTES) s.classification=ScanClass::Empty; return s; }
  s.classification=(size==HEADER_BYTES+s.records*RECORD_BYTES)?ScanClass::Complete:ScanClass::Recovered;
  return s;
}
} }
