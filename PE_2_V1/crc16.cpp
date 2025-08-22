#include <Arduino.h>
#include "bpm_estimator.h"

#define CRC16_POLY 0x8005
#define CRC16_INIT 0x0000
uint16_t culCalcCRC(uint8_t crcData, uint16_t crcReg) {
    for (uint8_t i = 0; i < 8; i++) {
        if (((crcReg & 0x8000) >> 8) ^ (crcData & 0x80))
            crcReg = (crcReg << 1) ^ CRC16_POLY;
        else
            crcReg = (crcReg << 1);
        crcData <<= 1;
    }
    return crcReg;
}

uint16_t computeCRC(const uint8_t* data, size_t length) {
  uint16_t crc = CRC16_INIT;
  for (size_t i = 0; i < length; i++) {
    crc = culCalcCRC(data[i], crc);
  }
  return crc;
}
