#include "I2cBus.h"
#include "Config.h"
#include <Wire.h>
namespace { bool ready=false; }
bool i2cBegin() {
  if (!ready) {
    ready=Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN,I2C_BUS_HZ);
    if (ready) Wire.setTimeOut(I2C_TIMEOUT_MS);
  }
  return ready;
}
bool i2cReady() { return ready; }
uint8_t i2cProbe(uint8_t a) {
  if (!ready) return 4;
  Wire.beginTransmission(a);
  return Wire.endTransmission(true);
}
bool i2cWrite(uint8_t a,const uint8_t* data,size_t n) {
  if (!ready) return false;
  Wire.beginTransmission(a);
  const bool queued=Wire.write(data,n)==n;
  const uint8_t err=Wire.endTransmission(true);
  return queued && err==0;
}
bool i2cRead(uint8_t a,uint8_t* data,size_t n) {
  if (!ready) return false;
  if (Wire.requestFrom(a,n,true)!=n) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i=0;i<n;++i) {
    const int v=Wire.read();
    if (v<0) return false;
    data[i]=uint8_t(v);
  }
  return true;
}
bool i2cReadRegs(uint8_t a,uint8_t reg,uint8_t* data,size_t n) {
  if (!ready) return false;
  Wire.beginTransmission(a);
  Wire.write(reg);
  return Wire.endTransmission(false)==0 && i2cRead(a,data,n);
}
