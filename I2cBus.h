#pragma once
#include <Arduino.h>
bool i2cBegin();
bool i2cReady();
uint8_t i2cProbe(uint8_t address);
bool i2cWrite(uint8_t address,const uint8_t* data,size_t n);
bool i2cRead(uint8_t address,uint8_t* data,size_t n);
bool i2cReadRegs(uint8_t address,uint8_t reg,uint8_t* data,size_t n);
