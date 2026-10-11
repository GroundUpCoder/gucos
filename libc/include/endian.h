
#pragma once
#include <stdint.h>
#include <byteswap.h>
/* wasm is always little-endian. */
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __PDP_ENDIAN    3412
#define __BYTE_ORDER    __LITTLE_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER
/* BSD-style unprefixed aliases (glibc exposes these under _GNU_SOURCE/_BSD_SOURCE). */
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN    __BIG_ENDIAN
#define PDP_ENDIAN    __PDP_ENDIAN
#define BYTE_ORDER    __BYTE_ORDER
static inline uint16_t htole16(uint16_t x) { return x; }
static inline uint16_t le16toh(uint16_t x) { return x; }
static inline uint32_t htole32(uint32_t x) { return x; }
static inline uint32_t le32toh(uint32_t x) { return x; }
static inline uint64_t htole64(uint64_t x) { return x; }
static inline uint64_t le64toh(uint64_t x) { return x; }
static inline uint16_t htobe16(uint16_t x) { return bswap_16(x); }
static inline uint16_t be16toh(uint16_t x) { return bswap_16(x); }
static inline uint32_t htobe32(uint32_t x) { return bswap_32(x); }
static inline uint32_t be32toh(uint32_t x) { return bswap_32(x); }
static inline uint64_t htobe64(uint64_t x) { return bswap_64(x); }
static inline uint64_t be64toh(uint64_t x) { return bswap_64(x); }
  