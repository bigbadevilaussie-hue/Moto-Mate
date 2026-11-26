#include "../LogCodec.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace mm::log;
static void fixCRC(uint8_t* p,size_t n) {
  uint32_t c=crc32(p,n-4); for(unsigned i=0;i<4;i++) p[n-4+i]=uint8_t(c>>(8*i));
}
int main() {
  assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
  const uint8_t expectedH[]={0x4d,0x4d,0x46,0x31,0x01,0x02,0x50,0x00,0x30,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x01,0x01,0x18,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x13,0x00,0x00,0x78,0x56,0x34,0x12,0xe8,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x5c,0x44,0x84,0x6f};
  const uint8_t expectedS[]={0x23,0xb3,0x71,0x17,0xa2,0x01,0x00,0x00,0x78,0x56,0x34,0x12,0x88,0x13,0x00,0x00,0x01,0x04,0xfd,0x03,0x78,0x37,0x83,0xff,0x50,0x14,0x8f,0x8b,0xd0,0xeb,0xcd,0xcb,0x20,0x5a,0x7b,0x02,0x2a,0x00,0xd2,0x04,0x09,0x80,0x00,0x00,0x93,0x77,0xa8,0x33};
  const uint8_t expectedL[]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x56,0x34,0x12,0x70,0x17,0x00,0x00,0x05,0x05,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0xe8,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x22,0x06,0x86,0xb1};
  Header h={2,0,{2,0,0,0,0,1},1,24,1234605616436508552ULL,2ULL,1ULL,1,1,0ULL,5000,305419896,1000};
  Prefix p={1795689657123ULL,305419896,5000,1,4,1021};
  Sample x={14200,-125,5200,-338654321,1512098765,635,42,1234,9,128,0};
  Prefix leg={0ULL,305419896,6000,5,5,0};
  Lifecycle event={1ULL,1,1,1,1000ULL};
  uint8_t header[HEADER_BYTES],r[RECORD_BYTES];
  assert(encodeHeader(h,header) && !memcmp(header,expectedH,sizeof(header)));
  assert(encode(p,x,r) && !memcmp(r,expectedS,sizeof(r)));
  assert(recordMatchesHeader(header,sizeof(header),r,sizeof(r)));
  assert(encode(leg,event,r) && !memcmp(r,expectedL,sizeof(r)));
  assert(recordMatchesHeader(header,sizeof(header),r,sizeof(r)));
  r[20]^=1; assert(!validRecord(r,sizeof(r)));
  assert(!validRecord(expectedS,sizeof(expectedS)-1));
  memcpy(r,expectedS,sizeof(r)); r[17]|=0x80; fixCRC(r,sizeof(r)); assert(!validRecord(r,sizeof(r)));
  memcpy(r,expectedS,sizeof(r)); r[18]&=~4u; fixCRC(r,sizeof(r)); assert(!validRecord(r,sizeof(r)));
  memcpy(r,expectedS,sizeof(r)); r[19]|=0x80; fixCRC(r,sizeof(r)); assert(!validRecord(r,sizeof(r)));
  memcpy(r,expectedS,sizeof(r)); r[17]|=0x40; fixCRC(r,sizeof(r)); assert(!recordMatchesHeader(header,sizeof(header),r,sizeof(r)));
  memcpy(r,expectedL,sizeof(r)); r[28]=2; fixCRC(r,sizeof(r)); assert(!recordMatchesHeader(header,sizeof(header),r,sizeof(r)));
  header[72]=1; fixCRC(header,sizeof(header)); assert(!validHeader(header,sizeof(header)));
  assert(encodeHeader(h,header));
  char path[PATH_BYTES];
  assert(filename(Kind::Ride,h.generation,h.file_seq,Suffix::Bin,path));
  assert(!strcmp(path,"/r1_11223344556677880000000000000002.bin"));
  assert(filenameMatchesHeader(path,header,sizeof(header)));
  path[35]='9'; assert(!filenameMatchesHeader(path,header,sizeof(header)));
  assert(filename(Kind::Ride,h.generation,h.file_seq,Suffix::GzipTemp,path) && strlen(path)==47);
  assert(!filename(Kind::Ride,0,1,Suffix::Bin,path));
  Identity id={h.generation,0},next={},decoded={}; uint64_t first=0;
  uint8_t raw[ID_BYTES]; assert(encodeIdentity(id,raw) && decodeIdentity(raw,sizeof(raw),decoded));
  assert(decoded.generation==id.generation && decoded.reservedThrough==0);
  assert(nextReservation(id,next,first) && first==1 && next.reservedThrough==32);
  id=next; assert(nextReservation(id,next,first) && first==33 && next.reservedThrough==64);
  assert(encodeIdentity(next,raw)); raw[4]^=1; assert(!decodeIdentity(raw,sizeof(raw),decoded));
  id.reservedThrough=UINT64_MAX-31; assert(!nextReservation(id,next,first));
  id.reservedThrough=1; assert(!encodeIdentity(id,raw));
  // Exercise each remaining typed payload and reserved/invalid checks.
  Prefix q={0,1,2,7,1,0};
  Boot boot={}; assert(encode(q,boot,r));
  q.type=8; ClockSync clock={0,0,INT64_MIN}; assert(encode(q,clock,r));
  q.type=9; Operation op={1,0,0,0,0,0,0}; assert(encode(q,op,r));
  q.type=10; Transfer tx={}; assert(encode(q,tx,r));
  q.type=11; Gap gap={0,0,0,0,UINT32_MAX,1}; assert(encode(q,gap,r));
  q.type=12; WakeSummary wake={}; assert(encode(q,wake,r));
  q.type=2; Sample missing={}; assert(encode(q,missing,r));
  q.type=3; Lifecycle trip={1,0,1,0,0}; assert(encode(q,trip,r));
  q.type=4; trip.leg=1; assert(encode(q,trip,r));
  q.type=6; trip.leg=0; assert(encode(q,trip,r));
  puts("log codec fixtures and negative cases PASS");
}
