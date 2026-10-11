
#pragma once
static inline unsigned short bswap_16(unsigned short x) {
  return (x >> 8) | (x << 8);
}
static inline unsigned int bswap_32(unsigned int x) {
  return (x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | (x << 24);
}
static inline unsigned long long bswap_64(unsigned long long x) {
  return ((x >> 56) & 0xFF) | ((x >> 40) & 0xFF00) |
         ((x >> 24) & 0xFF0000) | ((x >> 8) & 0xFF000000ULL) |
         ((x << 8) & 0xFF00000000ULL) | ((x << 24) & 0xFF0000000000ULL) |
         ((x << 40) & 0xFF000000000000ULL) | ((x << 56) & 0xFF00000000000000ULL);
}
  