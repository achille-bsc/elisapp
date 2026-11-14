#pragma once

#include "./endian.hpp"
#include <cstdint>

// CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no xor out).
// Same algorithm as the GPS packet (app/core/gps): one day both can share it.

// Constants //
constexpr uint32_t CRC16_INIT = 0xFFFF;
constexpr uint32_t CRC16_MASK = 0xFFFF;
constexpr uint32_t CRC16_POLY = 0x1021;
constexpr uint32_t CRC16_TOP_BIT = 0x8000;
constexpr uint16_t CRC16_SIZE = sizeof(uint16_t);

// Functions //
constexpr uint16_t crc16(const uint8_t *data, uint16_t len) {
  uint32_t crc = CRC16_INIT;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= static_cast<uint32_t>(data[i]) << BITS_PER_BYTE;
    for (uint32_t bit = 0; bit < BITS_PER_BYTE; bit++)
      crc = (crc & CRC16_TOP_BIT) ? (crc << 1) ^ CRC16_POLY : (crc << 1);
    crc &= CRC16_MASK;
  }
  return (static_cast<uint16_t>(crc));
}

// Standard check value of the CRC catalogue: crc("123456789") == 0x29B1
constexpr uint8_t CRC16_CHECK_INPUT[] = {'1', '2', '3', '4', '5',
                                         '6', '7', '8', '9'};
static_assert(crc16(CRC16_CHECK_INPUT, sizeof(CRC16_CHECK_INPUT)) == 0x29B1);
static_assert(crc16(CRC16_CHECK_INPUT, 0) == CRC16_INIT);
