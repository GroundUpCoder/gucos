#ifndef _WASM_SIMD128_H
#define _WASM_SIMD128_H
#ifndef __wasm_simd128__
#error wasm_simd128.h requires -msimd128
#endif
#include <stdint.h>
#include <stdbool.h>
typedef __v128 v128_t;
static inline __v128 wasm_v128_load(const void * __a0) { return __builtin(wasm_v128_load, __a0); }
#define wasm_v128_load(__a0) __builtin(wasm_v128_load, (__a0))
static inline __v128 wasm_i16x8_load8x8(const void * __a0) { return __builtin(wasm_i16x8_load8x8, __a0); }
#define wasm_i16x8_load8x8(__a0) __builtin(wasm_i16x8_load8x8, (__a0))
static inline __v128 wasm_u16x8_load8x8(const void * __a0) { return __builtin(wasm_u16x8_load8x8, __a0); }
#define wasm_u16x8_load8x8(__a0) __builtin(wasm_u16x8_load8x8, (__a0))
static inline __v128 wasm_i32x4_load16x4(const void * __a0) { return __builtin(wasm_i32x4_load16x4, __a0); }
#define wasm_i32x4_load16x4(__a0) __builtin(wasm_i32x4_load16x4, (__a0))
static inline __v128 wasm_u32x4_load16x4(const void * __a0) { return __builtin(wasm_u32x4_load16x4, __a0); }
#define wasm_u32x4_load16x4(__a0) __builtin(wasm_u32x4_load16x4, (__a0))
static inline __v128 wasm_i64x2_load32x2(const void * __a0) { return __builtin(wasm_i64x2_load32x2, __a0); }
#define wasm_i64x2_load32x2(__a0) __builtin(wasm_i64x2_load32x2, (__a0))
static inline __v128 wasm_u64x2_load32x2(const void * __a0) { return __builtin(wasm_u64x2_load32x2, __a0); }
#define wasm_u64x2_load32x2(__a0) __builtin(wasm_u64x2_load32x2, (__a0))
static inline __v128 wasm_v128_load8_splat(const void * __a0) { return __builtin(wasm_v128_load8_splat, __a0); }
#define wasm_v128_load8_splat(__a0) __builtin(wasm_v128_load8_splat, (__a0))
static inline __v128 wasm_v128_load16_splat(const void * __a0) { return __builtin(wasm_v128_load16_splat, __a0); }
#define wasm_v128_load16_splat(__a0) __builtin(wasm_v128_load16_splat, (__a0))
static inline __v128 wasm_v128_load32_splat(const void * __a0) { return __builtin(wasm_v128_load32_splat, __a0); }
#define wasm_v128_load32_splat(__a0) __builtin(wasm_v128_load32_splat, (__a0))
static inline __v128 wasm_v128_load64_splat(const void * __a0) { return __builtin(wasm_v128_load64_splat, __a0); }
#define wasm_v128_load64_splat(__a0) __builtin(wasm_v128_load64_splat, (__a0))
static inline void wasm_v128_store(void * __a0,__v128 __a1) { return __builtin(wasm_v128_store, __a0, __a1); }
#define wasm_v128_store(__a0,__a1) __builtin(wasm_v128_store, (__a0), (__a1))
#define wasm_i8x16_shuffle(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7,__a8,__a9,__a10,__a11,__a12,__a13,__a14,__a15,__a16,__a17) __builtin(wasm_i8x16_shuffle, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7), (__a8), (__a9), (__a10), (__a11), (__a12), (__a13), (__a14), (__a15), (__a16), (__a17))
static inline __v128 wasm_i8x16_swizzle(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_swizzle, __a0, __a1); }
#define wasm_i8x16_swizzle(__a0,__a1) __builtin(wasm_i8x16_swizzle, (__a0), (__a1))
static inline __v128 wasm_i8x16_splat(signed char __a0) { return __builtin(wasm_i8x16_splat, __a0); }
#define wasm_i8x16_splat(__a0) __builtin(wasm_i8x16_splat, (__a0))
static inline __v128 wasm_i16x8_splat(short __a0) { return __builtin(wasm_i16x8_splat, __a0); }
#define wasm_i16x8_splat(__a0) __builtin(wasm_i16x8_splat, (__a0))
static inline __v128 wasm_i32x4_splat(int __a0) { return __builtin(wasm_i32x4_splat, __a0); }
#define wasm_i32x4_splat(__a0) __builtin(wasm_i32x4_splat, (__a0))
static inline __v128 wasm_i64x2_splat(long long __a0) { return __builtin(wasm_i64x2_splat, __a0); }
#define wasm_i64x2_splat(__a0) __builtin(wasm_i64x2_splat, (__a0))
static inline __v128 wasm_f32x4_splat(float __a0) { return __builtin(wasm_f32x4_splat, __a0); }
#define wasm_f32x4_splat(__a0) __builtin(wasm_f32x4_splat, (__a0))
static inline __v128 wasm_f64x2_splat(double __a0) { return __builtin(wasm_f64x2_splat, __a0); }
#define wasm_f64x2_splat(__a0) __builtin(wasm_f64x2_splat, (__a0))
#define wasm_i8x16_extract_lane(__a0,__a1) __builtin(wasm_i8x16_extract_lane, (__a0), (__a1))
#define wasm_u8x16_extract_lane(__a0,__a1) __builtin(wasm_u8x16_extract_lane, (__a0), (__a1))
#define wasm_i8x16_replace_lane(__a0,__a1,__a2) __builtin(wasm_i8x16_replace_lane, (__a0), (__a2), (__a1))
#define wasm_i16x8_extract_lane(__a0,__a1) __builtin(wasm_i16x8_extract_lane, (__a0), (__a1))
#define wasm_u16x8_extract_lane(__a0,__a1) __builtin(wasm_u16x8_extract_lane, (__a0), (__a1))
#define wasm_i16x8_replace_lane(__a0,__a1,__a2) __builtin(wasm_i16x8_replace_lane, (__a0), (__a2), (__a1))
#define wasm_i32x4_extract_lane(__a0,__a1) __builtin(wasm_i32x4_extract_lane, (__a0), (__a1))
#define wasm_i32x4_replace_lane(__a0,__a1,__a2) __builtin(wasm_i32x4_replace_lane, (__a0), (__a2), (__a1))
#define wasm_i64x2_extract_lane(__a0,__a1) __builtin(wasm_i64x2_extract_lane, (__a0), (__a1))
#define wasm_i64x2_replace_lane(__a0,__a1,__a2) __builtin(wasm_i64x2_replace_lane, (__a0), (__a2), (__a1))
#define wasm_f32x4_extract_lane(__a0,__a1) __builtin(wasm_f32x4_extract_lane, (__a0), (__a1))
#define wasm_f32x4_replace_lane(__a0,__a1,__a2) __builtin(wasm_f32x4_replace_lane, (__a0), (__a2), (__a1))
#define wasm_f64x2_extract_lane(__a0,__a1) __builtin(wasm_f64x2_extract_lane, (__a0), (__a1))
#define wasm_f64x2_replace_lane(__a0,__a1,__a2) __builtin(wasm_f64x2_replace_lane, (__a0), (__a2), (__a1))
static inline __v128 wasm_i8x16_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_eq, __a0, __a1); }
#define wasm_i8x16_eq(__a0,__a1) __builtin(wasm_i8x16_eq, (__a0), (__a1))
static inline __v128 wasm_i8x16_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_ne, __a0, __a1); }
#define wasm_i8x16_ne(__a0,__a1) __builtin(wasm_i8x16_ne, (__a0), (__a1))
static inline __v128 wasm_i8x16_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_lt, __a0, __a1); }
#define wasm_i8x16_lt(__a0,__a1) __builtin(wasm_i8x16_lt, (__a0), (__a1))
static inline __v128 wasm_u8x16_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_lt, __a0, __a1); }
#define wasm_u8x16_lt(__a0,__a1) __builtin(wasm_u8x16_lt, (__a0), (__a1))
static inline __v128 wasm_i8x16_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_gt, __a0, __a1); }
#define wasm_i8x16_gt(__a0,__a1) __builtin(wasm_i8x16_gt, (__a0), (__a1))
static inline __v128 wasm_u8x16_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_gt, __a0, __a1); }
#define wasm_u8x16_gt(__a0,__a1) __builtin(wasm_u8x16_gt, (__a0), (__a1))
static inline __v128 wasm_i8x16_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_le, __a0, __a1); }
#define wasm_i8x16_le(__a0,__a1) __builtin(wasm_i8x16_le, (__a0), (__a1))
static inline __v128 wasm_u8x16_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_le, __a0, __a1); }
#define wasm_u8x16_le(__a0,__a1) __builtin(wasm_u8x16_le, (__a0), (__a1))
static inline __v128 wasm_i8x16_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_ge, __a0, __a1); }
#define wasm_i8x16_ge(__a0,__a1) __builtin(wasm_i8x16_ge, (__a0), (__a1))
static inline __v128 wasm_u8x16_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_ge, __a0, __a1); }
#define wasm_u8x16_ge(__a0,__a1) __builtin(wasm_u8x16_ge, (__a0), (__a1))
static inline __v128 wasm_i16x8_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_eq, __a0, __a1); }
#define wasm_i16x8_eq(__a0,__a1) __builtin(wasm_i16x8_eq, (__a0), (__a1))
static inline __v128 wasm_i16x8_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_ne, __a0, __a1); }
#define wasm_i16x8_ne(__a0,__a1) __builtin(wasm_i16x8_ne, (__a0), (__a1))
static inline __v128 wasm_i16x8_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_lt, __a0, __a1); }
#define wasm_i16x8_lt(__a0,__a1) __builtin(wasm_i16x8_lt, (__a0), (__a1))
static inline __v128 wasm_u16x8_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_lt, __a0, __a1); }
#define wasm_u16x8_lt(__a0,__a1) __builtin(wasm_u16x8_lt, (__a0), (__a1))
static inline __v128 wasm_i16x8_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_gt, __a0, __a1); }
#define wasm_i16x8_gt(__a0,__a1) __builtin(wasm_i16x8_gt, (__a0), (__a1))
static inline __v128 wasm_u16x8_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_gt, __a0, __a1); }
#define wasm_u16x8_gt(__a0,__a1) __builtin(wasm_u16x8_gt, (__a0), (__a1))
static inline __v128 wasm_i16x8_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_le, __a0, __a1); }
#define wasm_i16x8_le(__a0,__a1) __builtin(wasm_i16x8_le, (__a0), (__a1))
static inline __v128 wasm_u16x8_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_le, __a0, __a1); }
#define wasm_u16x8_le(__a0,__a1) __builtin(wasm_u16x8_le, (__a0), (__a1))
static inline __v128 wasm_i16x8_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_ge, __a0, __a1); }
#define wasm_i16x8_ge(__a0,__a1) __builtin(wasm_i16x8_ge, (__a0), (__a1))
static inline __v128 wasm_u16x8_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_ge, __a0, __a1); }
#define wasm_u16x8_ge(__a0,__a1) __builtin(wasm_u16x8_ge, (__a0), (__a1))
static inline __v128 wasm_i32x4_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_eq, __a0, __a1); }
#define wasm_i32x4_eq(__a0,__a1) __builtin(wasm_i32x4_eq, (__a0), (__a1))
static inline __v128 wasm_i32x4_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_ne, __a0, __a1); }
#define wasm_i32x4_ne(__a0,__a1) __builtin(wasm_i32x4_ne, (__a0), (__a1))
static inline __v128 wasm_i32x4_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_lt, __a0, __a1); }
#define wasm_i32x4_lt(__a0,__a1) __builtin(wasm_i32x4_lt, (__a0), (__a1))
static inline __v128 wasm_u32x4_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_lt, __a0, __a1); }
#define wasm_u32x4_lt(__a0,__a1) __builtin(wasm_u32x4_lt, (__a0), (__a1))
static inline __v128 wasm_i32x4_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_gt, __a0, __a1); }
#define wasm_i32x4_gt(__a0,__a1) __builtin(wasm_i32x4_gt, (__a0), (__a1))
static inline __v128 wasm_u32x4_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_gt, __a0, __a1); }
#define wasm_u32x4_gt(__a0,__a1) __builtin(wasm_u32x4_gt, (__a0), (__a1))
static inline __v128 wasm_i32x4_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_le, __a0, __a1); }
#define wasm_i32x4_le(__a0,__a1) __builtin(wasm_i32x4_le, (__a0), (__a1))
static inline __v128 wasm_u32x4_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_le, __a0, __a1); }
#define wasm_u32x4_le(__a0,__a1) __builtin(wasm_u32x4_le, (__a0), (__a1))
static inline __v128 wasm_i32x4_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_ge, __a0, __a1); }
#define wasm_i32x4_ge(__a0,__a1) __builtin(wasm_i32x4_ge, (__a0), (__a1))
static inline __v128 wasm_u32x4_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_ge, __a0, __a1); }
#define wasm_u32x4_ge(__a0,__a1) __builtin(wasm_u32x4_ge, (__a0), (__a1))
static inline __v128 wasm_f32x4_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_eq, __a0, __a1); }
#define wasm_f32x4_eq(__a0,__a1) __builtin(wasm_f32x4_eq, (__a0), (__a1))
static inline __v128 wasm_f32x4_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_ne, __a0, __a1); }
#define wasm_f32x4_ne(__a0,__a1) __builtin(wasm_f32x4_ne, (__a0), (__a1))
static inline __v128 wasm_f32x4_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_lt, __a0, __a1); }
#define wasm_f32x4_lt(__a0,__a1) __builtin(wasm_f32x4_lt, (__a0), (__a1))
static inline __v128 wasm_f32x4_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_gt, __a0, __a1); }
#define wasm_f32x4_gt(__a0,__a1) __builtin(wasm_f32x4_gt, (__a0), (__a1))
static inline __v128 wasm_f32x4_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_le, __a0, __a1); }
#define wasm_f32x4_le(__a0,__a1) __builtin(wasm_f32x4_le, (__a0), (__a1))
static inline __v128 wasm_f32x4_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_ge, __a0, __a1); }
#define wasm_f32x4_ge(__a0,__a1) __builtin(wasm_f32x4_ge, (__a0), (__a1))
static inline __v128 wasm_f64x2_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_eq, __a0, __a1); }
#define wasm_f64x2_eq(__a0,__a1) __builtin(wasm_f64x2_eq, (__a0), (__a1))
static inline __v128 wasm_f64x2_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_ne, __a0, __a1); }
#define wasm_f64x2_ne(__a0,__a1) __builtin(wasm_f64x2_ne, (__a0), (__a1))
static inline __v128 wasm_f64x2_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_lt, __a0, __a1); }
#define wasm_f64x2_lt(__a0,__a1) __builtin(wasm_f64x2_lt, (__a0), (__a1))
static inline __v128 wasm_f64x2_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_gt, __a0, __a1); }
#define wasm_f64x2_gt(__a0,__a1) __builtin(wasm_f64x2_gt, (__a0), (__a1))
static inline __v128 wasm_f64x2_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_le, __a0, __a1); }
#define wasm_f64x2_le(__a0,__a1) __builtin(wasm_f64x2_le, (__a0), (__a1))
static inline __v128 wasm_f64x2_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_ge, __a0, __a1); }
#define wasm_f64x2_ge(__a0,__a1) __builtin(wasm_f64x2_ge, (__a0), (__a1))
static inline __v128 wasm_v128_not(__v128 __a0) { return __builtin(wasm_v128_not, __a0); }
#define wasm_v128_not(__a0) __builtin(wasm_v128_not, (__a0))
static inline __v128 wasm_v128_and(__v128 __a0,__v128 __a1) { return __builtin(wasm_v128_and, __a0, __a1); }
#define wasm_v128_and(__a0,__a1) __builtin(wasm_v128_and, (__a0), (__a1))
static inline __v128 wasm_v128_andnot(__v128 __a0,__v128 __a1) { return __builtin(wasm_v128_andnot, __a0, __a1); }
#define wasm_v128_andnot(__a0,__a1) __builtin(wasm_v128_andnot, (__a0), (__a1))
static inline __v128 wasm_v128_or(__v128 __a0,__v128 __a1) { return __builtin(wasm_v128_or, __a0, __a1); }
#define wasm_v128_or(__a0,__a1) __builtin(wasm_v128_or, (__a0), (__a1))
static inline __v128 wasm_v128_xor(__v128 __a0,__v128 __a1) { return __builtin(wasm_v128_xor, __a0, __a1); }
#define wasm_v128_xor(__a0,__a1) __builtin(wasm_v128_xor, (__a0), (__a1))
static inline __v128 wasm_v128_bitselect(__v128 __a0,__v128 __a1,__v128 __a2) { return __builtin(wasm_v128_bitselect, __a0, __a1, __a2); }
#define wasm_v128_bitselect(__a0,__a1,__a2) __builtin(wasm_v128_bitselect, (__a0), (__a1), (__a2))
static inline _Bool wasm_v128_any_true(__v128 __a0) { return __builtin(wasm_v128_any_true, __a0); }
#define wasm_v128_any_true(__a0) __builtin(wasm_v128_any_true, (__a0))
#define wasm_v128_load8_lane(__a0,__a1,__a2) __builtin(wasm_v128_load8_lane, (__a0), (__a1), (__a2))
#define wasm_v128_load16_lane(__a0,__a1,__a2) __builtin(wasm_v128_load16_lane, (__a0), (__a1), (__a2))
#define wasm_v128_load32_lane(__a0,__a1,__a2) __builtin(wasm_v128_load32_lane, (__a0), (__a1), (__a2))
#define wasm_v128_load64_lane(__a0,__a1,__a2) __builtin(wasm_v128_load64_lane, (__a0), (__a1), (__a2))
#define wasm_v128_store8_lane(__a0,__a1,__a2) __builtin(wasm_v128_store8_lane, (__a0), (__a1), (__a2))
#define wasm_v128_store16_lane(__a0,__a1,__a2) __builtin(wasm_v128_store16_lane, (__a0), (__a1), (__a2))
#define wasm_v128_store32_lane(__a0,__a1,__a2) __builtin(wasm_v128_store32_lane, (__a0), (__a1), (__a2))
#define wasm_v128_store64_lane(__a0,__a1,__a2) __builtin(wasm_v128_store64_lane, (__a0), (__a1), (__a2))
static inline __v128 wasm_v128_load32_zero(const void * __a0) { return __builtin(wasm_v128_load32_zero, __a0); }
#define wasm_v128_load32_zero(__a0) __builtin(wasm_v128_load32_zero, (__a0))
static inline __v128 wasm_v128_load64_zero(const void * __a0) { return __builtin(wasm_v128_load64_zero, __a0); }
#define wasm_v128_load64_zero(__a0) __builtin(wasm_v128_load64_zero, (__a0))
static inline __v128 wasm_f32x4_demote_f64x2_zero(__v128 __a0) { return __builtin(wasm_f32x4_demote_f64x2_zero, __a0); }
#define wasm_f32x4_demote_f64x2_zero(__a0) __builtin(wasm_f32x4_demote_f64x2_zero, (__a0))
static inline __v128 wasm_f64x2_promote_low_f32x4(__v128 __a0) { return __builtin(wasm_f64x2_promote_low_f32x4, __a0); }
#define wasm_f64x2_promote_low_f32x4(__a0) __builtin(wasm_f64x2_promote_low_f32x4, (__a0))
static inline __v128 wasm_i8x16_abs(__v128 __a0) { return __builtin(wasm_i8x16_abs, __a0); }
#define wasm_i8x16_abs(__a0) __builtin(wasm_i8x16_abs, (__a0))
static inline __v128 wasm_i8x16_neg(__v128 __a0) { return __builtin(wasm_i8x16_neg, __a0); }
#define wasm_i8x16_neg(__a0) __builtin(wasm_i8x16_neg, (__a0))
static inline __v128 wasm_i8x16_popcnt(__v128 __a0) { return __builtin(wasm_i8x16_popcnt, __a0); }
#define wasm_i8x16_popcnt(__a0) __builtin(wasm_i8x16_popcnt, (__a0))
static inline _Bool wasm_i8x16_all_true(__v128 __a0) { return __builtin(wasm_i8x16_all_true, __a0); }
#define wasm_i8x16_all_true(__a0) __builtin(wasm_i8x16_all_true, (__a0))
static inline unsigned int wasm_i8x16_bitmask(__v128 __a0) { return __builtin(wasm_i8x16_bitmask, __a0); }
#define wasm_i8x16_bitmask(__a0) __builtin(wasm_i8x16_bitmask, (__a0))
static inline __v128 wasm_i8x16_narrow_i16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_narrow_i16x8, __a0, __a1); }
#define wasm_i8x16_narrow_i16x8(__a0,__a1) __builtin(wasm_i8x16_narrow_i16x8, (__a0), (__a1))
static inline __v128 wasm_u8x16_narrow_i16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_narrow_i16x8, __a0, __a1); }
#define wasm_u8x16_narrow_i16x8(__a0,__a1) __builtin(wasm_u8x16_narrow_i16x8, (__a0), (__a1))
static inline __v128 wasm_i8x16_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i8x16_shl, __a0, __a1); }
#define wasm_i8x16_shl(__a0,__a1) __builtin(wasm_i8x16_shl, (__a0), (__a1))
static inline __v128 wasm_i8x16_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i8x16_shr, __a0, __a1); }
#define wasm_i8x16_shr(__a0,__a1) __builtin(wasm_i8x16_shr, (__a0), (__a1))
static inline __v128 wasm_u8x16_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u8x16_shr, __a0, __a1); }
#define wasm_u8x16_shr(__a0,__a1) __builtin(wasm_u8x16_shr, (__a0), (__a1))
static inline __v128 wasm_i8x16_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_add, __a0, __a1); }
#define wasm_i8x16_add(__a0,__a1) __builtin(wasm_i8x16_add, (__a0), (__a1))
static inline __v128 wasm_i8x16_add_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_add_sat, __a0, __a1); }
#define wasm_i8x16_add_sat(__a0,__a1) __builtin(wasm_i8x16_add_sat, (__a0), (__a1))
static inline __v128 wasm_u8x16_add_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_add_sat, __a0, __a1); }
#define wasm_u8x16_add_sat(__a0,__a1) __builtin(wasm_u8x16_add_sat, (__a0), (__a1))
static inline __v128 wasm_i8x16_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_sub, __a0, __a1); }
#define wasm_i8x16_sub(__a0,__a1) __builtin(wasm_i8x16_sub, (__a0), (__a1))
static inline __v128 wasm_i8x16_sub_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_sub_sat, __a0, __a1); }
#define wasm_i8x16_sub_sat(__a0,__a1) __builtin(wasm_i8x16_sub_sat, (__a0), (__a1))
static inline __v128 wasm_u8x16_sub_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_sub_sat, __a0, __a1); }
#define wasm_u8x16_sub_sat(__a0,__a1) __builtin(wasm_u8x16_sub_sat, (__a0), (__a1))
static inline __v128 wasm_i8x16_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_min, __a0, __a1); }
#define wasm_i8x16_min(__a0,__a1) __builtin(wasm_i8x16_min, (__a0), (__a1))
static inline __v128 wasm_u8x16_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_min, __a0, __a1); }
#define wasm_u8x16_min(__a0,__a1) __builtin(wasm_u8x16_min, (__a0), (__a1))
static inline __v128 wasm_i8x16_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_i8x16_max, __a0, __a1); }
#define wasm_i8x16_max(__a0,__a1) __builtin(wasm_i8x16_max, (__a0), (__a1))
static inline __v128 wasm_u8x16_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_max, __a0, __a1); }
#define wasm_u8x16_max(__a0,__a1) __builtin(wasm_u8x16_max, (__a0), (__a1))
static inline __v128 wasm_u8x16_avgr(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_avgr, __a0, __a1); }
#define wasm_u8x16_avgr(__a0,__a1) __builtin(wasm_u8x16_avgr, (__a0), (__a1))
static inline __v128 wasm_i16x8_extadd_pairwise_i8x16(__v128 __a0) { return __builtin(wasm_i16x8_extadd_pairwise_i8x16, __a0); }
#define wasm_i16x8_extadd_pairwise_i8x16(__a0) __builtin(wasm_i16x8_extadd_pairwise_i8x16, (__a0))
static inline __v128 wasm_u16x8_extadd_pairwise_u8x16(__v128 __a0) { return __builtin(wasm_u16x8_extadd_pairwise_u8x16, __a0); }
#define wasm_u16x8_extadd_pairwise_u8x16(__a0) __builtin(wasm_u16x8_extadd_pairwise_u8x16, (__a0))
static inline __v128 wasm_i32x4_extadd_pairwise_i16x8(__v128 __a0) { return __builtin(wasm_i32x4_extadd_pairwise_i16x8, __a0); }
#define wasm_i32x4_extadd_pairwise_i16x8(__a0) __builtin(wasm_i32x4_extadd_pairwise_i16x8, (__a0))
static inline __v128 wasm_u32x4_extadd_pairwise_u16x8(__v128 __a0) { return __builtin(wasm_u32x4_extadd_pairwise_u16x8, __a0); }
#define wasm_u32x4_extadd_pairwise_u16x8(__a0) __builtin(wasm_u32x4_extadd_pairwise_u16x8, (__a0))
static inline __v128 wasm_i16x8_abs(__v128 __a0) { return __builtin(wasm_i16x8_abs, __a0); }
#define wasm_i16x8_abs(__a0) __builtin(wasm_i16x8_abs, (__a0))
static inline __v128 wasm_i16x8_neg(__v128 __a0) { return __builtin(wasm_i16x8_neg, __a0); }
#define wasm_i16x8_neg(__a0) __builtin(wasm_i16x8_neg, (__a0))
static inline __v128 wasm_i16x8_q15mulr_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_q15mulr_sat, __a0, __a1); }
#define wasm_i16x8_q15mulr_sat(__a0,__a1) __builtin(wasm_i16x8_q15mulr_sat, (__a0), (__a1))
static inline _Bool wasm_i16x8_all_true(__v128 __a0) { return __builtin(wasm_i16x8_all_true, __a0); }
#define wasm_i16x8_all_true(__a0) __builtin(wasm_i16x8_all_true, (__a0))
static inline unsigned int wasm_i16x8_bitmask(__v128 __a0) { return __builtin(wasm_i16x8_bitmask, __a0); }
#define wasm_i16x8_bitmask(__a0) __builtin(wasm_i16x8_bitmask, (__a0))
static inline __v128 wasm_i16x8_narrow_i32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_narrow_i32x4, __a0, __a1); }
#define wasm_i16x8_narrow_i32x4(__a0,__a1) __builtin(wasm_i16x8_narrow_i32x4, (__a0), (__a1))
static inline __v128 wasm_u16x8_narrow_i32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_narrow_i32x4, __a0, __a1); }
#define wasm_u16x8_narrow_i32x4(__a0,__a1) __builtin(wasm_u16x8_narrow_i32x4, (__a0), (__a1))
static inline __v128 wasm_i16x8_extend_low_i8x16(__v128 __a0) { return __builtin(wasm_i16x8_extend_low_i8x16, __a0); }
#define wasm_i16x8_extend_low_i8x16(__a0) __builtin(wasm_i16x8_extend_low_i8x16, (__a0))
static inline __v128 wasm_i16x8_extend_high_i8x16(__v128 __a0) { return __builtin(wasm_i16x8_extend_high_i8x16, __a0); }
#define wasm_i16x8_extend_high_i8x16(__a0) __builtin(wasm_i16x8_extend_high_i8x16, (__a0))
static inline __v128 wasm_u16x8_extend_low_u8x16(__v128 __a0) { return __builtin(wasm_u16x8_extend_low_u8x16, __a0); }
#define wasm_u16x8_extend_low_u8x16(__a0) __builtin(wasm_u16x8_extend_low_u8x16, (__a0))
static inline __v128 wasm_u16x8_extend_high_u8x16(__v128 __a0) { return __builtin(wasm_u16x8_extend_high_u8x16, __a0); }
#define wasm_u16x8_extend_high_u8x16(__a0) __builtin(wasm_u16x8_extend_high_u8x16, (__a0))
static inline __v128 wasm_i16x8_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i16x8_shl, __a0, __a1); }
#define wasm_i16x8_shl(__a0,__a1) __builtin(wasm_i16x8_shl, (__a0), (__a1))
static inline __v128 wasm_i16x8_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i16x8_shr, __a0, __a1); }
#define wasm_i16x8_shr(__a0,__a1) __builtin(wasm_i16x8_shr, (__a0), (__a1))
static inline __v128 wasm_u16x8_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u16x8_shr, __a0, __a1); }
#define wasm_u16x8_shr(__a0,__a1) __builtin(wasm_u16x8_shr, (__a0), (__a1))
static inline __v128 wasm_i16x8_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_add, __a0, __a1); }
#define wasm_i16x8_add(__a0,__a1) __builtin(wasm_i16x8_add, (__a0), (__a1))
static inline __v128 wasm_i16x8_add_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_add_sat, __a0, __a1); }
#define wasm_i16x8_add_sat(__a0,__a1) __builtin(wasm_i16x8_add_sat, (__a0), (__a1))
static inline __v128 wasm_u16x8_add_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_add_sat, __a0, __a1); }
#define wasm_u16x8_add_sat(__a0,__a1) __builtin(wasm_u16x8_add_sat, (__a0), (__a1))
static inline __v128 wasm_i16x8_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_sub, __a0, __a1); }
#define wasm_i16x8_sub(__a0,__a1) __builtin(wasm_i16x8_sub, (__a0), (__a1))
static inline __v128 wasm_i16x8_sub_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_sub_sat, __a0, __a1); }
#define wasm_i16x8_sub_sat(__a0,__a1) __builtin(wasm_i16x8_sub_sat, (__a0), (__a1))
static inline __v128 wasm_u16x8_sub_sat(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_sub_sat, __a0, __a1); }
#define wasm_u16x8_sub_sat(__a0,__a1) __builtin(wasm_u16x8_sub_sat, (__a0), (__a1))
static inline __v128 wasm_i16x8_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_mul, __a0, __a1); }
#define wasm_i16x8_mul(__a0,__a1) __builtin(wasm_i16x8_mul, (__a0), (__a1))
static inline __v128 wasm_i16x8_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_min, __a0, __a1); }
#define wasm_i16x8_min(__a0,__a1) __builtin(wasm_i16x8_min, (__a0), (__a1))
static inline __v128 wasm_u16x8_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_min, __a0, __a1); }
#define wasm_u16x8_min(__a0,__a1) __builtin(wasm_u16x8_min, (__a0), (__a1))
static inline __v128 wasm_i16x8_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_max, __a0, __a1); }
#define wasm_i16x8_max(__a0,__a1) __builtin(wasm_i16x8_max, (__a0), (__a1))
static inline __v128 wasm_u16x8_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_max, __a0, __a1); }
#define wasm_u16x8_max(__a0,__a1) __builtin(wasm_u16x8_max, (__a0), (__a1))
static inline __v128 wasm_u16x8_avgr(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_avgr, __a0, __a1); }
#define wasm_u16x8_avgr(__a0,__a1) __builtin(wasm_u16x8_avgr, (__a0), (__a1))
static inline __v128 wasm_i16x8_extmul_low_i8x16(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_extmul_low_i8x16, __a0, __a1); }
#define wasm_i16x8_extmul_low_i8x16(__a0,__a1) __builtin(wasm_i16x8_extmul_low_i8x16, (__a0), (__a1))
static inline __v128 wasm_i16x8_extmul_high_i8x16(__v128 __a0,__v128 __a1) { return __builtin(wasm_i16x8_extmul_high_i8x16, __a0, __a1); }
#define wasm_i16x8_extmul_high_i8x16(__a0,__a1) __builtin(wasm_i16x8_extmul_high_i8x16, (__a0), (__a1))
static inline __v128 wasm_u16x8_extmul_low_u8x16(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_extmul_low_u8x16, __a0, __a1); }
#define wasm_u16x8_extmul_low_u8x16(__a0,__a1) __builtin(wasm_u16x8_extmul_low_u8x16, (__a0), (__a1))
static inline __v128 wasm_u16x8_extmul_high_u8x16(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_extmul_high_u8x16, __a0, __a1); }
#define wasm_u16x8_extmul_high_u8x16(__a0,__a1) __builtin(wasm_u16x8_extmul_high_u8x16, (__a0), (__a1))
static inline __v128 wasm_i32x4_abs(__v128 __a0) { return __builtin(wasm_i32x4_abs, __a0); }
#define wasm_i32x4_abs(__a0) __builtin(wasm_i32x4_abs, (__a0))
static inline __v128 wasm_i32x4_neg(__v128 __a0) { return __builtin(wasm_i32x4_neg, __a0); }
#define wasm_i32x4_neg(__a0) __builtin(wasm_i32x4_neg, (__a0))
static inline _Bool wasm_i32x4_all_true(__v128 __a0) { return __builtin(wasm_i32x4_all_true, __a0); }
#define wasm_i32x4_all_true(__a0) __builtin(wasm_i32x4_all_true, (__a0))
static inline unsigned int wasm_i32x4_bitmask(__v128 __a0) { return __builtin(wasm_i32x4_bitmask, __a0); }
#define wasm_i32x4_bitmask(__a0) __builtin(wasm_i32x4_bitmask, (__a0))
static inline __v128 wasm_i32x4_extend_low_i16x8(__v128 __a0) { return __builtin(wasm_i32x4_extend_low_i16x8, __a0); }
#define wasm_i32x4_extend_low_i16x8(__a0) __builtin(wasm_i32x4_extend_low_i16x8, (__a0))
static inline __v128 wasm_i32x4_extend_high_i16x8(__v128 __a0) { return __builtin(wasm_i32x4_extend_high_i16x8, __a0); }
#define wasm_i32x4_extend_high_i16x8(__a0) __builtin(wasm_i32x4_extend_high_i16x8, (__a0))
static inline __v128 wasm_u32x4_extend_low_u16x8(__v128 __a0) { return __builtin(wasm_u32x4_extend_low_u16x8, __a0); }
#define wasm_u32x4_extend_low_u16x8(__a0) __builtin(wasm_u32x4_extend_low_u16x8, (__a0))
static inline __v128 wasm_u32x4_extend_high_u16x8(__v128 __a0) { return __builtin(wasm_u32x4_extend_high_u16x8, __a0); }
#define wasm_u32x4_extend_high_u16x8(__a0) __builtin(wasm_u32x4_extend_high_u16x8, (__a0))
static inline __v128 wasm_i32x4_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i32x4_shl, __a0, __a1); }
#define wasm_i32x4_shl(__a0,__a1) __builtin(wasm_i32x4_shl, (__a0), (__a1))
static inline __v128 wasm_i32x4_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i32x4_shr, __a0, __a1); }
#define wasm_i32x4_shr(__a0,__a1) __builtin(wasm_i32x4_shr, (__a0), (__a1))
static inline __v128 wasm_u32x4_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u32x4_shr, __a0, __a1); }
#define wasm_u32x4_shr(__a0,__a1) __builtin(wasm_u32x4_shr, (__a0), (__a1))
static inline __v128 wasm_i32x4_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_add, __a0, __a1); }
#define wasm_i32x4_add(__a0,__a1) __builtin(wasm_i32x4_add, (__a0), (__a1))
static inline __v128 wasm_i32x4_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_sub, __a0, __a1); }
#define wasm_i32x4_sub(__a0,__a1) __builtin(wasm_i32x4_sub, (__a0), (__a1))
static inline __v128 wasm_i32x4_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_mul, __a0, __a1); }
#define wasm_i32x4_mul(__a0,__a1) __builtin(wasm_i32x4_mul, (__a0), (__a1))
static inline __v128 wasm_i32x4_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_min, __a0, __a1); }
#define wasm_i32x4_min(__a0,__a1) __builtin(wasm_i32x4_min, (__a0), (__a1))
static inline __v128 wasm_u32x4_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_min, __a0, __a1); }
#define wasm_u32x4_min(__a0,__a1) __builtin(wasm_u32x4_min, (__a0), (__a1))
static inline __v128 wasm_i32x4_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_max, __a0, __a1); }
#define wasm_i32x4_max(__a0,__a1) __builtin(wasm_i32x4_max, (__a0), (__a1))
static inline __v128 wasm_u32x4_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_max, __a0, __a1); }
#define wasm_u32x4_max(__a0,__a1) __builtin(wasm_u32x4_max, (__a0), (__a1))
static inline __v128 wasm_i32x4_dot_i16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_dot_i16x8, __a0, __a1); }
#define wasm_i32x4_dot_i16x8(__a0,__a1) __builtin(wasm_i32x4_dot_i16x8, (__a0), (__a1))
static inline __v128 wasm_i32x4_extmul_low_i16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_extmul_low_i16x8, __a0, __a1); }
#define wasm_i32x4_extmul_low_i16x8(__a0,__a1) __builtin(wasm_i32x4_extmul_low_i16x8, (__a0), (__a1))
static inline __v128 wasm_i32x4_extmul_high_i16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_i32x4_extmul_high_i16x8, __a0, __a1); }
#define wasm_i32x4_extmul_high_i16x8(__a0,__a1) __builtin(wasm_i32x4_extmul_high_i16x8, (__a0), (__a1))
static inline __v128 wasm_u32x4_extmul_low_u16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_extmul_low_u16x8, __a0, __a1); }
#define wasm_u32x4_extmul_low_u16x8(__a0,__a1) __builtin(wasm_u32x4_extmul_low_u16x8, (__a0), (__a1))
static inline __v128 wasm_u32x4_extmul_high_u16x8(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_extmul_high_u16x8, __a0, __a1); }
#define wasm_u32x4_extmul_high_u16x8(__a0,__a1) __builtin(wasm_u32x4_extmul_high_u16x8, (__a0), (__a1))
static inline __v128 wasm_i64x2_abs(__v128 __a0) { return __builtin(wasm_i64x2_abs, __a0); }
#define wasm_i64x2_abs(__a0) __builtin(wasm_i64x2_abs, (__a0))
static inline __v128 wasm_i64x2_neg(__v128 __a0) { return __builtin(wasm_i64x2_neg, __a0); }
#define wasm_i64x2_neg(__a0) __builtin(wasm_i64x2_neg, (__a0))
static inline _Bool wasm_i64x2_all_true(__v128 __a0) { return __builtin(wasm_i64x2_all_true, __a0); }
#define wasm_i64x2_all_true(__a0) __builtin(wasm_i64x2_all_true, (__a0))
static inline unsigned int wasm_i64x2_bitmask(__v128 __a0) { return __builtin(wasm_i64x2_bitmask, __a0); }
#define wasm_i64x2_bitmask(__a0) __builtin(wasm_i64x2_bitmask, (__a0))
static inline __v128 wasm_i64x2_extend_low_i32x4(__v128 __a0) { return __builtin(wasm_i64x2_extend_low_i32x4, __a0); }
#define wasm_i64x2_extend_low_i32x4(__a0) __builtin(wasm_i64x2_extend_low_i32x4, (__a0))
static inline __v128 wasm_i64x2_extend_high_i32x4(__v128 __a0) { return __builtin(wasm_i64x2_extend_high_i32x4, __a0); }
#define wasm_i64x2_extend_high_i32x4(__a0) __builtin(wasm_i64x2_extend_high_i32x4, (__a0))
static inline __v128 wasm_u64x2_extend_low_u32x4(__v128 __a0) { return __builtin(wasm_u64x2_extend_low_u32x4, __a0); }
#define wasm_u64x2_extend_low_u32x4(__a0) __builtin(wasm_u64x2_extend_low_u32x4, (__a0))
static inline __v128 wasm_u64x2_extend_high_u32x4(__v128 __a0) { return __builtin(wasm_u64x2_extend_high_u32x4, __a0); }
#define wasm_u64x2_extend_high_u32x4(__a0) __builtin(wasm_u64x2_extend_high_u32x4, (__a0))
static inline __v128 wasm_i64x2_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i64x2_shl, __a0, __a1); }
#define wasm_i64x2_shl(__a0,__a1) __builtin(wasm_i64x2_shl, (__a0), (__a1))
static inline __v128 wasm_i64x2_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_i64x2_shr, __a0, __a1); }
#define wasm_i64x2_shr(__a0,__a1) __builtin(wasm_i64x2_shr, (__a0), (__a1))
static inline __v128 wasm_u64x2_shr(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u64x2_shr, __a0, __a1); }
#define wasm_u64x2_shr(__a0,__a1) __builtin(wasm_u64x2_shr, (__a0), (__a1))
static inline __v128 wasm_i64x2_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_add, __a0, __a1); }
#define wasm_i64x2_add(__a0,__a1) __builtin(wasm_i64x2_add, (__a0), (__a1))
static inline __v128 wasm_i64x2_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_sub, __a0, __a1); }
#define wasm_i64x2_sub(__a0,__a1) __builtin(wasm_i64x2_sub, (__a0), (__a1))
static inline __v128 wasm_i64x2_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_mul, __a0, __a1); }
#define wasm_i64x2_mul(__a0,__a1) __builtin(wasm_i64x2_mul, (__a0), (__a1))
static inline __v128 wasm_i64x2_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_eq, __a0, __a1); }
#define wasm_i64x2_eq(__a0,__a1) __builtin(wasm_i64x2_eq, (__a0), (__a1))
static inline __v128 wasm_i64x2_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_ne, __a0, __a1); }
#define wasm_i64x2_ne(__a0,__a1) __builtin(wasm_i64x2_ne, (__a0), (__a1))
static inline __v128 wasm_i64x2_lt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_lt, __a0, __a1); }
#define wasm_i64x2_lt(__a0,__a1) __builtin(wasm_i64x2_lt, (__a0), (__a1))
static inline __v128 wasm_i64x2_gt(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_gt, __a0, __a1); }
#define wasm_i64x2_gt(__a0,__a1) __builtin(wasm_i64x2_gt, (__a0), (__a1))
static inline __v128 wasm_i64x2_le(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_le, __a0, __a1); }
#define wasm_i64x2_le(__a0,__a1) __builtin(wasm_i64x2_le, (__a0), (__a1))
static inline __v128 wasm_i64x2_ge(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_ge, __a0, __a1); }
#define wasm_i64x2_ge(__a0,__a1) __builtin(wasm_i64x2_ge, (__a0), (__a1))
static inline __v128 wasm_i64x2_extmul_low_i32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_extmul_low_i32x4, __a0, __a1); }
#define wasm_i64x2_extmul_low_i32x4(__a0,__a1) __builtin(wasm_i64x2_extmul_low_i32x4, (__a0), (__a1))
static inline __v128 wasm_i64x2_extmul_high_i32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_i64x2_extmul_high_i32x4, __a0, __a1); }
#define wasm_i64x2_extmul_high_i32x4(__a0,__a1) __builtin(wasm_i64x2_extmul_high_i32x4, (__a0), (__a1))
static inline __v128 wasm_u64x2_extmul_low_u32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_extmul_low_u32x4, __a0, __a1); }
#define wasm_u64x2_extmul_low_u32x4(__a0,__a1) __builtin(wasm_u64x2_extmul_low_u32x4, (__a0), (__a1))
static inline __v128 wasm_u64x2_extmul_high_u32x4(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_extmul_high_u32x4, __a0, __a1); }
#define wasm_u64x2_extmul_high_u32x4(__a0,__a1) __builtin(wasm_u64x2_extmul_high_u32x4, (__a0), (__a1))
static inline __v128 wasm_f32x4_ceil(__v128 __a0) { return __builtin(wasm_f32x4_ceil, __a0); }
#define wasm_f32x4_ceil(__a0) __builtin(wasm_f32x4_ceil, (__a0))
static inline __v128 wasm_f32x4_floor(__v128 __a0) { return __builtin(wasm_f32x4_floor, __a0); }
#define wasm_f32x4_floor(__a0) __builtin(wasm_f32x4_floor, (__a0))
static inline __v128 wasm_f32x4_trunc(__v128 __a0) { return __builtin(wasm_f32x4_trunc, __a0); }
#define wasm_f32x4_trunc(__a0) __builtin(wasm_f32x4_trunc, (__a0))
static inline __v128 wasm_f32x4_nearest(__v128 __a0) { return __builtin(wasm_f32x4_nearest, __a0); }
#define wasm_f32x4_nearest(__a0) __builtin(wasm_f32x4_nearest, (__a0))
static inline __v128 wasm_f64x2_ceil(__v128 __a0) { return __builtin(wasm_f64x2_ceil, __a0); }
#define wasm_f64x2_ceil(__a0) __builtin(wasm_f64x2_ceil, (__a0))
static inline __v128 wasm_f64x2_floor(__v128 __a0) { return __builtin(wasm_f64x2_floor, __a0); }
#define wasm_f64x2_floor(__a0) __builtin(wasm_f64x2_floor, (__a0))
static inline __v128 wasm_f64x2_trunc(__v128 __a0) { return __builtin(wasm_f64x2_trunc, __a0); }
#define wasm_f64x2_trunc(__a0) __builtin(wasm_f64x2_trunc, (__a0))
static inline __v128 wasm_f64x2_nearest(__v128 __a0) { return __builtin(wasm_f64x2_nearest, __a0); }
#define wasm_f64x2_nearest(__a0) __builtin(wasm_f64x2_nearest, (__a0))
static inline __v128 wasm_f32x4_abs(__v128 __a0) { return __builtin(wasm_f32x4_abs, __a0); }
#define wasm_f32x4_abs(__a0) __builtin(wasm_f32x4_abs, (__a0))
static inline __v128 wasm_f32x4_neg(__v128 __a0) { return __builtin(wasm_f32x4_neg, __a0); }
#define wasm_f32x4_neg(__a0) __builtin(wasm_f32x4_neg, (__a0))
static inline __v128 wasm_f32x4_sqrt(__v128 __a0) { return __builtin(wasm_f32x4_sqrt, __a0); }
#define wasm_f32x4_sqrt(__a0) __builtin(wasm_f32x4_sqrt, (__a0))
static inline __v128 wasm_f32x4_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_add, __a0, __a1); }
#define wasm_f32x4_add(__a0,__a1) __builtin(wasm_f32x4_add, (__a0), (__a1))
static inline __v128 wasm_f32x4_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_sub, __a0, __a1); }
#define wasm_f32x4_sub(__a0,__a1) __builtin(wasm_f32x4_sub, (__a0), (__a1))
static inline __v128 wasm_f32x4_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_mul, __a0, __a1); }
#define wasm_f32x4_mul(__a0,__a1) __builtin(wasm_f32x4_mul, (__a0), (__a1))
static inline __v128 wasm_f32x4_div(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_div, __a0, __a1); }
#define wasm_f32x4_div(__a0,__a1) __builtin(wasm_f32x4_div, (__a0), (__a1))
static inline __v128 wasm_f32x4_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_min, __a0, __a1); }
#define wasm_f32x4_min(__a0,__a1) __builtin(wasm_f32x4_min, (__a0), (__a1))
static inline __v128 wasm_f32x4_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_max, __a0, __a1); }
#define wasm_f32x4_max(__a0,__a1) __builtin(wasm_f32x4_max, (__a0), (__a1))
static inline __v128 wasm_f32x4_pmin(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_pmin, __a0, __a1); }
#define wasm_f32x4_pmin(__a0,__a1) __builtin(wasm_f32x4_pmin, (__a0), (__a1))
static inline __v128 wasm_f32x4_pmax(__v128 __a0,__v128 __a1) { return __builtin(wasm_f32x4_pmax, __a0, __a1); }
#define wasm_f32x4_pmax(__a0,__a1) __builtin(wasm_f32x4_pmax, (__a0), (__a1))
static inline __v128 wasm_f64x2_abs(__v128 __a0) { return __builtin(wasm_f64x2_abs, __a0); }
#define wasm_f64x2_abs(__a0) __builtin(wasm_f64x2_abs, (__a0))
static inline __v128 wasm_f64x2_neg(__v128 __a0) { return __builtin(wasm_f64x2_neg, __a0); }
#define wasm_f64x2_neg(__a0) __builtin(wasm_f64x2_neg, (__a0))
static inline __v128 wasm_f64x2_sqrt(__v128 __a0) { return __builtin(wasm_f64x2_sqrt, __a0); }
#define wasm_f64x2_sqrt(__a0) __builtin(wasm_f64x2_sqrt, (__a0))
static inline __v128 wasm_f64x2_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_add, __a0, __a1); }
#define wasm_f64x2_add(__a0,__a1) __builtin(wasm_f64x2_add, (__a0), (__a1))
static inline __v128 wasm_f64x2_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_sub, __a0, __a1); }
#define wasm_f64x2_sub(__a0,__a1) __builtin(wasm_f64x2_sub, (__a0), (__a1))
static inline __v128 wasm_f64x2_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_mul, __a0, __a1); }
#define wasm_f64x2_mul(__a0,__a1) __builtin(wasm_f64x2_mul, (__a0), (__a1))
static inline __v128 wasm_f64x2_div(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_div, __a0, __a1); }
#define wasm_f64x2_div(__a0,__a1) __builtin(wasm_f64x2_div, (__a0), (__a1))
static inline __v128 wasm_f64x2_min(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_min, __a0, __a1); }
#define wasm_f64x2_min(__a0,__a1) __builtin(wasm_f64x2_min, (__a0), (__a1))
static inline __v128 wasm_f64x2_max(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_max, __a0, __a1); }
#define wasm_f64x2_max(__a0,__a1) __builtin(wasm_f64x2_max, (__a0), (__a1))
static inline __v128 wasm_f64x2_pmin(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_pmin, __a0, __a1); }
#define wasm_f64x2_pmin(__a0,__a1) __builtin(wasm_f64x2_pmin, (__a0), (__a1))
static inline __v128 wasm_f64x2_pmax(__v128 __a0,__v128 __a1) { return __builtin(wasm_f64x2_pmax, __a0, __a1); }
#define wasm_f64x2_pmax(__a0,__a1) __builtin(wasm_f64x2_pmax, (__a0), (__a1))
static inline __v128 wasm_i32x4_trunc_sat_f32x4(__v128 __a0) { return __builtin(wasm_i32x4_trunc_sat_f32x4, __a0); }
#define wasm_i32x4_trunc_sat_f32x4(__a0) __builtin(wasm_i32x4_trunc_sat_f32x4, (__a0))
static inline __v128 wasm_u32x4_trunc_sat_f32x4(__v128 __a0) { return __builtin(wasm_u32x4_trunc_sat_f32x4, __a0); }
#define wasm_u32x4_trunc_sat_f32x4(__a0) __builtin(wasm_u32x4_trunc_sat_f32x4, (__a0))
static inline __v128 wasm_f32x4_convert_i32x4(__v128 __a0) { return __builtin(wasm_f32x4_convert_i32x4, __a0); }
#define wasm_f32x4_convert_i32x4(__a0) __builtin(wasm_f32x4_convert_i32x4, (__a0))
static inline __v128 wasm_f32x4_convert_u32x4(__v128 __a0) { return __builtin(wasm_f32x4_convert_u32x4, __a0); }
#define wasm_f32x4_convert_u32x4(__a0) __builtin(wasm_f32x4_convert_u32x4, (__a0))
static inline __v128 wasm_i32x4_trunc_sat_f64x2_zero(__v128 __a0) { return __builtin(wasm_i32x4_trunc_sat_f64x2_zero, __a0); }
#define wasm_i32x4_trunc_sat_f64x2_zero(__a0) __builtin(wasm_i32x4_trunc_sat_f64x2_zero, (__a0))
static inline __v128 wasm_u32x4_trunc_sat_f64x2_zero(__v128 __a0) { return __builtin(wasm_u32x4_trunc_sat_f64x2_zero, __a0); }
#define wasm_u32x4_trunc_sat_f64x2_zero(__a0) __builtin(wasm_u32x4_trunc_sat_f64x2_zero, (__a0))
static inline __v128 wasm_f64x2_convert_low_i32x4(__v128 __a0) { return __builtin(wasm_f64x2_convert_low_i32x4, __a0); }
#define wasm_f64x2_convert_low_i32x4(__a0) __builtin(wasm_f64x2_convert_low_i32x4, (__a0))
static inline __v128 wasm_f64x2_convert_low_u32x4(__v128 __a0) { return __builtin(wasm_f64x2_convert_low_u32x4, __a0); }
#define wasm_f64x2_convert_low_u32x4(__a0) __builtin(wasm_f64x2_convert_low_u32x4, (__a0))
static inline __v128 wasm_u8x16_splat(unsigned char __a0) { return __builtin(wasm_u8x16_splat, __a0); }
#define wasm_u8x16_splat(__a0) __builtin(wasm_u8x16_splat, (__a0))
static inline __v128 wasm_u16x8_splat(unsigned short __a0) { return __builtin(wasm_u16x8_splat, __a0); }
#define wasm_u16x8_splat(__a0) __builtin(wasm_u16x8_splat, (__a0))
static inline __v128 wasm_u32x4_splat(unsigned int __a0) { return __builtin(wasm_u32x4_splat, __a0); }
#define wasm_u32x4_splat(__a0) __builtin(wasm_u32x4_splat, (__a0))
static inline __v128 wasm_u64x2_splat(unsigned long long __a0) { return __builtin(wasm_u64x2_splat, __a0); }
#define wasm_u64x2_splat(__a0) __builtin(wasm_u64x2_splat, (__a0))
#define wasm_u8x16_replace_lane(__a0,__a1,__a2) __builtin(wasm_u8x16_replace_lane, (__a0), (__a2), (__a1))
#define wasm_u16x8_replace_lane(__a0,__a1,__a2) __builtin(wasm_u16x8_replace_lane, (__a0), (__a2), (__a1))
#define wasm_u32x4_extract_lane(__a0,__a1) __builtin(wasm_u32x4_extract_lane, (__a0), (__a1))
#define wasm_u32x4_replace_lane(__a0,__a1,__a2) __builtin(wasm_u32x4_replace_lane, (__a0), (__a2), (__a1))
#define wasm_u64x2_extract_lane(__a0,__a1) __builtin(wasm_u64x2_extract_lane, (__a0), (__a1))
#define wasm_u64x2_replace_lane(__a0,__a1,__a2) __builtin(wasm_u64x2_replace_lane, (__a0), (__a2), (__a1))
static inline __v128 wasm_u8x16_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_eq, __a0, __a1); }
#define wasm_u8x16_eq(__a0,__a1) __builtin(wasm_u8x16_eq, (__a0), (__a1))
static inline __v128 wasm_u8x16_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_ne, __a0, __a1); }
#define wasm_u8x16_ne(__a0,__a1) __builtin(wasm_u8x16_ne, (__a0), (__a1))
static inline __v128 wasm_u16x8_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_eq, __a0, __a1); }
#define wasm_u16x8_eq(__a0,__a1) __builtin(wasm_u16x8_eq, (__a0), (__a1))
static inline __v128 wasm_u16x8_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_ne, __a0, __a1); }
#define wasm_u16x8_ne(__a0,__a1) __builtin(wasm_u16x8_ne, (__a0), (__a1))
static inline __v128 wasm_u32x4_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_eq, __a0, __a1); }
#define wasm_u32x4_eq(__a0,__a1) __builtin(wasm_u32x4_eq, (__a0), (__a1))
static inline __v128 wasm_u32x4_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_ne, __a0, __a1); }
#define wasm_u32x4_ne(__a0,__a1) __builtin(wasm_u32x4_ne, (__a0), (__a1))
static inline _Bool wasm_u8x16_all_true(__v128 __a0) { return __builtin(wasm_u8x16_all_true, __a0); }
#define wasm_u8x16_all_true(__a0) __builtin(wasm_u8x16_all_true, (__a0))
static inline unsigned int wasm_u8x16_bitmask(__v128 __a0) { return __builtin(wasm_u8x16_bitmask, __a0); }
#define wasm_u8x16_bitmask(__a0) __builtin(wasm_u8x16_bitmask, (__a0))
static inline __v128 wasm_u8x16_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u8x16_shl, __a0, __a1); }
#define wasm_u8x16_shl(__a0,__a1) __builtin(wasm_u8x16_shl, (__a0), (__a1))
static inline __v128 wasm_u8x16_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_add, __a0, __a1); }
#define wasm_u8x16_add(__a0,__a1) __builtin(wasm_u8x16_add, (__a0), (__a1))
static inline __v128 wasm_u8x16_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_u8x16_sub, __a0, __a1); }
#define wasm_u8x16_sub(__a0,__a1) __builtin(wasm_u8x16_sub, (__a0), (__a1))
static inline _Bool wasm_u16x8_all_true(__v128 __a0) { return __builtin(wasm_u16x8_all_true, __a0); }
#define wasm_u16x8_all_true(__a0) __builtin(wasm_u16x8_all_true, (__a0))
static inline unsigned int wasm_u16x8_bitmask(__v128 __a0) { return __builtin(wasm_u16x8_bitmask, __a0); }
#define wasm_u16x8_bitmask(__a0) __builtin(wasm_u16x8_bitmask, (__a0))
static inline __v128 wasm_u16x8_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u16x8_shl, __a0, __a1); }
#define wasm_u16x8_shl(__a0,__a1) __builtin(wasm_u16x8_shl, (__a0), (__a1))
static inline __v128 wasm_u16x8_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_add, __a0, __a1); }
#define wasm_u16x8_add(__a0,__a1) __builtin(wasm_u16x8_add, (__a0), (__a1))
static inline __v128 wasm_u16x8_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_sub, __a0, __a1); }
#define wasm_u16x8_sub(__a0,__a1) __builtin(wasm_u16x8_sub, (__a0), (__a1))
static inline __v128 wasm_u16x8_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_u16x8_mul, __a0, __a1); }
#define wasm_u16x8_mul(__a0,__a1) __builtin(wasm_u16x8_mul, (__a0), (__a1))
static inline _Bool wasm_u32x4_all_true(__v128 __a0) { return __builtin(wasm_u32x4_all_true, __a0); }
#define wasm_u32x4_all_true(__a0) __builtin(wasm_u32x4_all_true, (__a0))
static inline unsigned int wasm_u32x4_bitmask(__v128 __a0) { return __builtin(wasm_u32x4_bitmask, __a0); }
#define wasm_u32x4_bitmask(__a0) __builtin(wasm_u32x4_bitmask, (__a0))
static inline __v128 wasm_u32x4_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u32x4_shl, __a0, __a1); }
#define wasm_u32x4_shl(__a0,__a1) __builtin(wasm_u32x4_shl, (__a0), (__a1))
static inline __v128 wasm_u32x4_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_add, __a0, __a1); }
#define wasm_u32x4_add(__a0,__a1) __builtin(wasm_u32x4_add, (__a0), (__a1))
static inline __v128 wasm_u32x4_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_sub, __a0, __a1); }
#define wasm_u32x4_sub(__a0,__a1) __builtin(wasm_u32x4_sub, (__a0), (__a1))
static inline __v128 wasm_u32x4_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_u32x4_mul, __a0, __a1); }
#define wasm_u32x4_mul(__a0,__a1) __builtin(wasm_u32x4_mul, (__a0), (__a1))
static inline _Bool wasm_u64x2_all_true(__v128 __a0) { return __builtin(wasm_u64x2_all_true, __a0); }
#define wasm_u64x2_all_true(__a0) __builtin(wasm_u64x2_all_true, (__a0))
static inline unsigned int wasm_u64x2_bitmask(__v128 __a0) { return __builtin(wasm_u64x2_bitmask, __a0); }
#define wasm_u64x2_bitmask(__a0) __builtin(wasm_u64x2_bitmask, (__a0))
static inline __v128 wasm_u64x2_shl(__v128 __a0,unsigned int __a1) { return __builtin(wasm_u64x2_shl, __a0, __a1); }
#define wasm_u64x2_shl(__a0,__a1) __builtin(wasm_u64x2_shl, (__a0), (__a1))
static inline __v128 wasm_u64x2_add(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_add, __a0, __a1); }
#define wasm_u64x2_add(__a0,__a1) __builtin(wasm_u64x2_add, (__a0), (__a1))
static inline __v128 wasm_u64x2_sub(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_sub, __a0, __a1); }
#define wasm_u64x2_sub(__a0,__a1) __builtin(wasm_u64x2_sub, (__a0), (__a1))
static inline __v128 wasm_u64x2_mul(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_mul, __a0, __a1); }
#define wasm_u64x2_mul(__a0,__a1) __builtin(wasm_u64x2_mul, (__a0), (__a1))
static inline __v128 wasm_u64x2_eq(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_eq, __a0, __a1); }
#define wasm_u64x2_eq(__a0,__a1) __builtin(wasm_u64x2_eq, (__a0), (__a1))
static inline __v128 wasm_u64x2_ne(__v128 __a0,__v128 __a1) { return __builtin(wasm_u64x2_ne, __a0, __a1); }
#define wasm_u64x2_ne(__a0,__a1) __builtin(wasm_u64x2_ne, (__a0), (__a1))
static inline __v128 wasm_i8x16_make(signed char __a0,signed char __a1,signed char __a2,signed char __a3,signed char __a4,signed char __a5,signed char __a6,signed char __a7,signed char __a8,signed char __a9,signed char __a10,signed char __a11,signed char __a12,signed char __a13,signed char __a14,signed char __a15) { return __builtin(wasm_i8x16_make, __a0, __a1, __a2, __a3, __a4, __a5, __a6, __a7, __a8, __a9, __a10, __a11, __a12, __a13, __a14, __a15); }
#define wasm_i8x16_make(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7,__a8,__a9,__a10,__a11,__a12,__a13,__a14,__a15) __builtin(wasm_i8x16_make, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7), (__a8), (__a9), (__a10), (__a11), (__a12), (__a13), (__a14), (__a15))
#define wasm_i8x16_const(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7,__a8,__a9,__a10,__a11,__a12,__a13,__a14,__a15) __builtin(wasm_i8x16_const, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7), (__a8), (__a9), (__a10), (__a11), (__a12), (__a13), (__a14), (__a15))
#define wasm_i8x16_const_splat(__a0) __builtin(wasm_i8x16_const_splat, (__a0))
static inline __v128 wasm_u8x16_make(unsigned char __a0,unsigned char __a1,unsigned char __a2,unsigned char __a3,unsigned char __a4,unsigned char __a5,unsigned char __a6,unsigned char __a7,unsigned char __a8,unsigned char __a9,unsigned char __a10,unsigned char __a11,unsigned char __a12,unsigned char __a13,unsigned char __a14,unsigned char __a15) { return __builtin(wasm_u8x16_make, __a0, __a1, __a2, __a3, __a4, __a5, __a6, __a7, __a8, __a9, __a10, __a11, __a12, __a13, __a14, __a15); }
#define wasm_u8x16_make(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7,__a8,__a9,__a10,__a11,__a12,__a13,__a14,__a15) __builtin(wasm_u8x16_make, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7), (__a8), (__a9), (__a10), (__a11), (__a12), (__a13), (__a14), (__a15))
#define wasm_u8x16_const(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7,__a8,__a9,__a10,__a11,__a12,__a13,__a14,__a15) __builtin(wasm_u8x16_const, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7), (__a8), (__a9), (__a10), (__a11), (__a12), (__a13), (__a14), (__a15))
#define wasm_u8x16_const_splat(__a0) __builtin(wasm_u8x16_const_splat, (__a0))
static inline __v128 wasm_i16x8_make(short __a0,short __a1,short __a2,short __a3,short __a4,short __a5,short __a6,short __a7) { return __builtin(wasm_i16x8_make, __a0, __a1, __a2, __a3, __a4, __a5, __a6, __a7); }
#define wasm_i16x8_make(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7) __builtin(wasm_i16x8_make, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7))
#define wasm_i16x8_const(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7) __builtin(wasm_i16x8_const, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7))
#define wasm_i16x8_const_splat(__a0) __builtin(wasm_i16x8_const_splat, (__a0))
static inline __v128 wasm_u16x8_make(unsigned short __a0,unsigned short __a1,unsigned short __a2,unsigned short __a3,unsigned short __a4,unsigned short __a5,unsigned short __a6,unsigned short __a7) { return __builtin(wasm_u16x8_make, __a0, __a1, __a2, __a3, __a4, __a5, __a6, __a7); }
#define wasm_u16x8_make(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7) __builtin(wasm_u16x8_make, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7))
#define wasm_u16x8_const(__a0,__a1,__a2,__a3,__a4,__a5,__a6,__a7) __builtin(wasm_u16x8_const, (__a0), (__a1), (__a2), (__a3), (__a4), (__a5), (__a6), (__a7))
#define wasm_u16x8_const_splat(__a0) __builtin(wasm_u16x8_const_splat, (__a0))
static inline __v128 wasm_i32x4_make(int __a0,int __a1,int __a2,int __a3) { return __builtin(wasm_i32x4_make, __a0, __a1, __a2, __a3); }
#define wasm_i32x4_make(__a0,__a1,__a2,__a3) __builtin(wasm_i32x4_make, (__a0), (__a1), (__a2), (__a3))
#define wasm_i32x4_const(__a0,__a1,__a2,__a3) __builtin(wasm_i32x4_const, (__a0), (__a1), (__a2), (__a3))
#define wasm_i32x4_const_splat(__a0) __builtin(wasm_i32x4_const_splat, (__a0))
static inline __v128 wasm_u32x4_make(unsigned int __a0,unsigned int __a1,unsigned int __a2,unsigned int __a3) { return __builtin(wasm_u32x4_make, __a0, __a1, __a2, __a3); }
#define wasm_u32x4_make(__a0,__a1,__a2,__a3) __builtin(wasm_u32x4_make, (__a0), (__a1), (__a2), (__a3))
#define wasm_u32x4_const(__a0,__a1,__a2,__a3) __builtin(wasm_u32x4_const, (__a0), (__a1), (__a2), (__a3))
#define wasm_u32x4_const_splat(__a0) __builtin(wasm_u32x4_const_splat, (__a0))
static inline __v128 wasm_i64x2_make(long long __a0,long long __a1) { return __builtin(wasm_i64x2_make, __a0, __a1); }
#define wasm_i64x2_make(__a0,__a1) __builtin(wasm_i64x2_make, (__a0), (__a1))
#define wasm_i64x2_const(__a0,__a1) __builtin(wasm_i64x2_const, (__a0), (__a1))
#define wasm_i64x2_const_splat(__a0) __builtin(wasm_i64x2_const_splat, (__a0))
static inline __v128 wasm_u64x2_make(unsigned long long __a0,unsigned long long __a1) { return __builtin(wasm_u64x2_make, __a0, __a1); }
#define wasm_u64x2_make(__a0,__a1) __builtin(wasm_u64x2_make, (__a0), (__a1))
#define wasm_u64x2_const(__a0,__a1) __builtin(wasm_u64x2_const, (__a0), (__a1))
#define wasm_u64x2_const_splat(__a0) __builtin(wasm_u64x2_const_splat, (__a0))
static inline __v128 wasm_f32x4_make(float __a0,float __a1,float __a2,float __a3) { return __builtin(wasm_f32x4_make, __a0, __a1, __a2, __a3); }
#define wasm_f32x4_make(__a0,__a1,__a2,__a3) __builtin(wasm_f32x4_make, (__a0), (__a1), (__a2), (__a3))
#define wasm_f32x4_const(__a0,__a1,__a2,__a3) __builtin(wasm_f32x4_const, (__a0), (__a1), (__a2), (__a3))
#define wasm_f32x4_const_splat(__a0) __builtin(wasm_f32x4_const_splat, (__a0))
static inline __v128 wasm_f64x2_make(double __a0,double __a1) { return __builtin(wasm_f64x2_make, __a0, __a1); }
#define wasm_f64x2_make(__a0,__a1) __builtin(wasm_f64x2_make, (__a0), (__a1))
#define wasm_f64x2_const(__a0,__a1) __builtin(wasm_f64x2_const, (__a0), (__a1))
#define wasm_f64x2_const_splat(__a0) __builtin(wasm_f64x2_const_splat, (__a0))
#define wasm_i16x8_shuffle(__a,__b,__c0,__c1,__c2,__c3,__c4,__c5,__c6,__c7) wasm_i8x16_shuffle((__a),(__b),((__c0)*2+0),((__c0)*2+1),((__c1)*2+0),((__c1)*2+1),((__c2)*2+0),((__c2)*2+1),((__c3)*2+0),((__c3)*2+1),((__c4)*2+0),((__c4)*2+1),((__c5)*2+0),((__c5)*2+1),((__c6)*2+0),((__c6)*2+1),((__c7)*2+0),((__c7)*2+1))
#define wasm_i32x4_shuffle(__a,__b,__c0,__c1,__c2,__c3) wasm_i8x16_shuffle((__a),(__b),((__c0)*4+0),((__c0)*4+1),((__c0)*4+2),((__c0)*4+3),((__c1)*4+0),((__c1)*4+1),((__c1)*4+2),((__c1)*4+3),((__c2)*4+0),((__c2)*4+1),((__c2)*4+2),((__c2)*4+3),((__c3)*4+0),((__c3)*4+1),((__c3)*4+2),((__c3)*4+3))
#define wasm_i64x2_shuffle(__a,__b,__c0,__c1) wasm_i8x16_shuffle((__a),(__b),((__c0)*8+0),((__c0)*8+1),((__c0)*8+2),((__c0)*8+3),((__c0)*8+4),((__c0)*8+5),((__c0)*8+6),((__c0)*8+7),((__c1)*8+0),((__c1)*8+1),((__c1)*8+2),((__c1)*8+3),((__c1)*8+4),((__c1)*8+5),((__c1)*8+6),((__c1)*8+7))
#endif
