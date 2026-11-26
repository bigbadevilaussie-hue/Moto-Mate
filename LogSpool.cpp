#include "LogSpool.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace mm { namespace log {
namespace {
constexpr size_t FULL_BYTES=64;
bool fullPath(const char* relative,char* out) {
  const int n=snprintf(out,FULL_BYTES,"/littlefs%s",relative);
  return n>0 && size_t(n)<FULL_BYTES;
}
bool absent(const char* path) {
  struct stat s={}; return stat(path,&s)!=0 && errno==ENOENT;
}
bool allAbsent(Kind k,uint64_t g,uint64_t seq,bool exceptOpen) {
  char relative[PATH_BYTES],full[FULL_BYTES];
  for(unsigned i=exceptOpen?1:0;i<6;++i) {
    if(!filename(k,g,seq,static_cast<Suffix>(i),relative) || !fullPath(relative,full) || !absent(full)) return false;
  }
  return true;
}
bool readExact(int fd,uint8_t* b,size_t n) {
  // No retry loop on failed I/O. Preserve original bytes and report fault.
  return read(fd,b,n)==static_cast<ssize_t>(n);
}
bool finishWrite(int fd,bool written) {
  const bool sync=written && fsync(fd)==0;
  const bool closed=close(fd)==0;
  return sync && closed;
}
bool verifyRange(const char* path,off_t offset,const uint8_t* expected,size_t n,size_t total) {
  uint8_t b[HEADER_BYTES]; struct stat s={};
  if(n>sizeof(b)) return false;
  const int fd=open(path,O_RDONLY);
  if(fd<0) return false;
  const bool ok=fstat(fd,&s)==0 && s.st_size==static_cast<off_t>(total) &&
    lseek(fd,offset,SEEK_SET)==offset && readExact(fd,b,n) && !memcmp(b,expected,n);
  return close(fd)==0 && ok;
}
Scan inspectFile(const char* relative,const char* full) {
  Scan s={ScanClass::IoError,0,0};
  const int fd=open(full,O_RDONLY);
  if(fd<0) return s;
  struct stat st={};
  uint8_t h[HEADER_BYTES],r[RECORD_BYTES];
  if(fstat(fd,&st)==0 && st.st_size>=0 && S_ISREG(st.st_mode)) {
    s.bytes=static_cast<size_t>(st.st_size);
    if(s.bytes<HEADER_BYTES) s.classification=ScanClass::Bad;
    else if(readExact(fd,h,sizeof(h))) {
      if(!filenameMatchesHeader(relative,h,sizeof(h))) s.classification=ScanClass::Bad;
      else {
        bool io=true;
        while(s.records<MAX_RECORDS && s.bytes-HEADER_BYTES-s.records*RECORD_BYTES>=RECORD_BYTES) {
          if(!readExact(fd,r,sizeof(r))) { io=false; break; }
          if(!recordMatchesHeader(h,sizeof(h),r,sizeof(r))) break;
          ++s.records;
        }
        if(io) {
          if(!s.records) s.classification=s.bytes==HEADER_BYTES?ScanClass::Empty:ScanClass::Bad;
          else s.classification=s.bytes==HEADER_BYTES+s.records*RECORD_BYTES?ScanClass::Complete:ScanClass::Recovered;
        }
      }
    }
  }
  if(close(fd)!=0) s.classification=ScanClass::IoError;
  return s;
}
}
bool benchIdentityAbsent(uint64_t g,uint64_t seq) {
  return allAbsent(Kind::Monitor,g,seq,false) && allAbsent(Kind::Ride,g,seq,false);
}
bool inspectSealedMonitor(uint64_t g,uint64_t seq,Scan& result) {
  if(!allAbsent(Kind::Ride,g,seq,false)) return false;
  char rel[PATH_BYTES],full[FULL_BYTES];
  if(!filename(Kind::Monitor,g,seq,Suffix::Bin,rel) || !fullPath(rel,full)) return false;
  // Refuse any ambiguous sibling, including an active or compressed copy.
  for(unsigned i=0;i<6;++i) {
    if(i==static_cast<unsigned>(Suffix::Bin)) continue;
    char other[PATH_BYTES],otherFull[FULL_BYTES];
    if(!filename(Kind::Monitor,g,seq,static_cast<Suffix>(i),other) ||
       !fullPath(other,otherFull) || !absent(otherFull)) return false;
  }
  result=inspectFile(rel,full);
  return result.classification==ScanClass::Complete && result.bytes<=MAX_RAW_BYTES;
}
bool createOpen(const Header& h) {
  uint8_t b[HEADER_BYTES]; char rel[PATH_BYTES],full[FULL_BYTES];
  const Kind k=static_cast<Kind>(h.kind);
  if(!encodeHeader(h,b) || !filename(k,h.generation,h.file_seq,Suffix::Open,rel) ||
     !fullPath(rel,full) || !allAbsent(k,h.generation,h.file_seq,false)) return false;
  const int fd=open(full,O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd<0) return false;
  return finishWrite(fd,write(fd,b,sizeof(b))==sizeof(b)) && verifyRange(full,0,b,sizeof(b),sizeof(b));
}
bool appendOne(const Header& h,size_t count,const uint8_t* record) {
  uint8_t b[HEADER_BYTES]; char rel[PATH_BYTES],full[FULL_BYTES];
  const Kind k=static_cast<Kind>(h.kind);
  if(count>=MAX_RECORDS || !encodeHeader(h,b) || !recordMatchesHeader(b,sizeof(b),record,RECORD_BYTES) ||
     !filename(k,h.generation,h.file_seq,Suffix::Open,rel) || !fullPath(rel,full) ||
     !allAbsent(k,h.generation,h.file_seq,true)) return false;
  const size_t offset=HEADER_BYTES+count*RECORD_BYTES;
  // Caller owns verified count and never resumes an interrupted open file.
  if(!verifyRange(full,0,b,sizeof(b),offset)) return false;
  const int fd=open(full,O_WRONLY|O_APPEND);
  if(fd<0) return false;
  return finishWrite(fd,write(fd,record,RECORD_BYTES)==RECORD_BYTES) &&
    verifyRange(full,offset,record,RECORD_BYTES,offset+RECORD_BYTES);
}
bool recoverOpen(Kind k,uint64_t g,uint64_t seq,Scan& result,Suffix& suffix) {
  char rel[PATH_BYTES],source[FULL_BYTES],dstRel[PATH_BYTES],dst[FULL_BYTES];
  if(!filename(k,g,seq,Suffix::Open,rel) || !fullPath(rel,source) || !allAbsent(k,g,seq,true)) return false;
  result=inspectFile(rel,source);
  if(result.classification==ScanClass::IoError) return false;
  // Preserve even header-only files as .bad; no deletion in this slice.
  suffix=result.classification==ScanClass::Complete?Suffix::Bin:
    result.classification==ScanClass::Recovered?Suffix::Recovered:Suffix::Bad;
  if(!filename(k,g,seq,suffix,dstRel) || !fullPath(dstRel,dst) || !absent(dst)) return false;
  // Single cooperative owner; no competing writer. LittleFS rename is atomic
  // at filesystem level; actual power-cut durability still requires bench test.
  if(rename(source,dst)!=0 || !absent(source)) return false;
  const Scan verified=inspectFile(dstRel,dst);
  return verified.classification==result.classification && verified.bytes==result.bytes &&
    verified.records==result.records;
}
} }
