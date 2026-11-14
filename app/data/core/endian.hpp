#pragma once

#include <cstdint>

// Little endian byte access, alignment free: frames are read in place from
// radio buffers that have no alignment guarantee.

// Constants //
constexpr uint32_t BITS_PER_BYTE = 8;
constexpr uint32_t BYTE_MASK = 0xFF;
constexpr uint32_t U16_MASK = 0xFFFF;
constexpr uint32_t U16_BITS = 16;

// Functions //
constexpr void put_u16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & BYTE_MASK);
  p[1] = static_cast<uint8_t>(v >> BITS_PER_BYTE);
}

constexpr void put_u32(uint8_t *p, uint32_t v) {
  put_u16(p, static_cast<uint16_t>(v & U16_MASK));
  put_u16(p + sizeof(uint16_t), static_cast<uint16_t>(v >> U16_BITS));
}

constexpr uint16_t get_u16(const uint8_t *p) {
  return (static_cast<uint16_t>(p[0] | (p[1] << BITS_PER_BYTE)));
}

constexpr uint32_t get_u32(const uint8_t *p) {
  return (get_u16(p) |
          (static_cast<uint32_t>(get_u16(p + sizeof(uint16_t))) << U16_BITS));
}

consteval bool endian_roundtrip_ok() {
  uint8_t buf[sizeof(uint32_t)] = {};
  put_u32(buf, 0x12345678);
  return (buf[0] == 0x78 && buf[3] == 0x12 && get_u32(buf) == 0x12345678 &&
          get_u16(buf) == 0x5678);
}

static_assert(endian_roundtrip_ok(), "little endian, least byte first");
