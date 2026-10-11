#ifndef _EMMINTRIN_H
#define _EMMINTRIN_H
#ifndef __SSE2__
#error this header requires -msse2
#endif
#include <xmmintrin.h>
typedef v128_t __m128d;
static inline __attribute__((always_inline)) __m128d _mm_setzero_pd(void) { return wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128d _mm_set1_pd(double x) { return wasm_f64x2_splat(x); }
#define _mm_set_pd1 _mm_set1_pd
static inline __attribute__((always_inline)) __m128d _mm_set_pd(double a,double b) { return wasm_f64x2_make(b,a); }
static inline __attribute__((always_inline)) __m128d _mm_setr_pd(double a,double b) { return wasm_f64x2_make(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_set_sd(double x) { return wasm_f64x2_replace_lane(wasm_i32x4_const_splat(0),0,x); }
static inline __attribute__((always_inline)) __m128d _mm_move_sd(__m128d a,__m128d b) { return wasm_f64x2_replace_lane(a,0,wasm_f64x2_extract_lane(b,0)); }
static inline __attribute__((always_inline)) __m128d _mm_add_pd(__m128d a, __m128d b) { return wasm_f64x2_add(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_add_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_add_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_sub_pd(__m128d a, __m128d b) { return wasm_f64x2_sub(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_sub_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_sub_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_mul_pd(__m128d a, __m128d b) { return wasm_f64x2_mul(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_mul_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_mul_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_div_pd(__m128d a, __m128d b) { return wasm_f64x2_div(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_div_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_div_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_sqrt_pd(__m128d a) { return wasm_f64x2_sqrt(a); }
static inline __attribute__((always_inline)) __m128d _mm_sqrt_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_sqrt_pd(b)); }
static inline __attribute__((always_inline)) __m128d _mm_min_pd(__m128d a,__m128d b) { return wasm_v128_bitselect(a,b,wasm_f64x2_lt(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_min_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_min_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_max_pd(__m128d a,__m128d b) { return wasm_v128_bitselect(a,b,wasm_f64x2_gt(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_max_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_max_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_and_pd(__m128d a, __m128d b) { return wasm_v128_and(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_or_pd(__m128d a, __m128d b) { return wasm_v128_or(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_xor_pd(__m128d a, __m128d b) { return wasm_v128_xor(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_andnot_pd(__m128d a,__m128d b) { return wasm_v128_andnot(b,a); }
static inline __attribute__((always_inline)) __m128d _mm_cmpeq_pd(__m128d a,__m128d b) { return wasm_f64x2_eq(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmpeq_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpeq_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmplt_pd(__m128d a,__m128d b) { return wasm_f64x2_lt(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmplt_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmplt_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmple_pd(__m128d a,__m128d b) { return wasm_f64x2_le(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmple_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmple_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpgt_pd(__m128d a,__m128d b) { return wasm_f64x2_gt(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmpgt_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpgt_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpge_pd(__m128d a,__m128d b) { return wasm_f64x2_ge(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmpge_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpge_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpneq_pd(__m128d a,__m128d b) { return wasm_f64x2_ne(a,b); }
static inline __attribute__((always_inline)) __m128d _mm_cmpneq_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpneq_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnlt_pd(__m128d a,__m128d b) { return wasm_v128_not(wasm_f64x2_lt(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnlt_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpnlt_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnle_pd(__m128d a,__m128d b) { return wasm_v128_not(wasm_f64x2_le(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnle_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpnle_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpngt_pd(__m128d a,__m128d b) { return wasm_v128_not(wasm_f64x2_gt(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpngt_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpngt_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnge_pd(__m128d a,__m128d b) { return wasm_v128_not(wasm_f64x2_ge(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpnge_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpnge_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpord_pd(__m128d a,__m128d b) { return wasm_v128_and(wasm_f64x2_eq(a,a),wasm_f64x2_eq(b,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpunord_pd(__m128d a,__m128d b) { return wasm_v128_not(_mm_cmpord_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpord_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpord_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_cmpunord_sd(__m128d a,__m128d b) { return _mm_move_sd(a,_mm_cmpunord_pd(a,b)); }
static inline __attribute__((always_inline)) __m128d _mm_loadu_pd(const double *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) __m128d _mm_load_pd(const double *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) __m128d _mm_load_sd(const double *p) { return wasm_v128_load64_zero(p); }
static inline __attribute__((always_inline)) __m128d _mm_load1_pd(const double *p) { return wasm_v128_load64_splat(p); }
#define _mm_load_pd1 _mm_load1_pd
static inline __attribute__((always_inline)) void _mm_store_pd(double *p,__m128d a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_storeu_pd(double *p,__m128d a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_stream_pd(double *p,__m128d a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_store_sd(double *p,__m128d a) { wasm_v128_store64_lane(p,a,0); }
static inline __attribute__((always_inline)) void _mm_store1_pd(double *p,__m128d a) { wasm_v128_store(p,wasm_f64x2_splat(wasm_f64x2_extract_lane(a,0))); }
#define _mm_store_pd1 _mm_store1_pd
static inline __attribute__((always_inline)) int _mm_movemask_pd(__m128d a) { return wasm_i64x2_bitmask(a); }
static inline __attribute__((always_inline)) double _mm_cvtsd_f64(__m128d a) { return wasm_f64x2_extract_lane(a,0); }
static inline __attribute__((always_inline)) __m128d _mm_unpacklo_pd(__m128d a,__m128d b) { return wasm_i64x2_shuffle(a,b,0,2); }
static inline __attribute__((always_inline)) __m128d _mm_unpackhi_pd(__m128d a,__m128d b) { return wasm_i64x2_shuffle(a,b,1,3); }
static inline __attribute__((always_inline)) __m128d _mm_undefined_pd(void) { return wasm_i32x4_const_splat(0); }
#define _mm_shuffle_pd(a,b,m) wasm_i64x2_shuffle((a),(b),((m)&1),((((m)>>1)&1)+2))
static inline __attribute__((always_inline)) __m128i _mm_set1_epi8(char x) { return wasm_i8x16_splat(x); }
static inline __attribute__((always_inline)) __m128i _mm_set_epi8(char x0,char x1,char x2,char x3,char x4,char x5,char x6,char x7,char x8,char x9,char x10,char x11,char x12,char x13,char x14,char x15) { return wasm_i8x16_make(x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,x0); }
static inline __attribute__((always_inline)) __m128i _mm_setr_epi8(char x0,char x1,char x2,char x3,char x4,char x5,char x6,char x7,char x8,char x9,char x10,char x11,char x12,char x13,char x14,char x15) { return wasm_i8x16_make(x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15); }
static inline __attribute__((always_inline)) __m128i _mm_add_epi8(__m128i a, __m128i b) { return wasm_i8x16_add(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_sub_epi8(__m128i a, __m128i b) { return wasm_i8x16_sub(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpeq_epi8(__m128i a, __m128i b) { return wasm_i8x16_eq(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpgt_epi8(__m128i a, __m128i b) { return wasm_i8x16_gt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmplt_epi8(__m128i a, __m128i b) { return wasm_i8x16_lt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_adds_epi8(__m128i a, __m128i b) { return wasm_i8x16_add_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_adds_epu8(__m128i a, __m128i b) { return wasm_u8x16_add_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_subs_epi8(__m128i a, __m128i b) { return wasm_i8x16_sub_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_subs_epu8(__m128i a, __m128i b) { return wasm_u8x16_sub_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_unpacklo_epi8(__m128i a,__m128i b) { return wasm_i8x16_shuffle(a,b,0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23); }
static inline __attribute__((always_inline)) __m128i _mm_unpackhi_epi8(__m128i a,__m128i b) { return wasm_i8x16_shuffle(a,b,8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31); }
static inline __attribute__((always_inline)) __m128i _mm_set1_epi16(short x) { return wasm_i16x8_splat(x); }
static inline __attribute__((always_inline)) __m128i _mm_set_epi16(short x0,short x1,short x2,short x3,short x4,short x5,short x6,short x7) { return wasm_i16x8_make(x7,x6,x5,x4,x3,x2,x1,x0); }
static inline __attribute__((always_inline)) __m128i _mm_setr_epi16(short x0,short x1,short x2,short x3,short x4,short x5,short x6,short x7) { return wasm_i16x8_make(x0,x1,x2,x3,x4,x5,x6,x7); }
static inline __attribute__((always_inline)) __m128i _mm_add_epi16(__m128i a, __m128i b) { return wasm_i16x8_add(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_sub_epi16(__m128i a, __m128i b) { return wasm_i16x8_sub(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpeq_epi16(__m128i a, __m128i b) { return wasm_i16x8_eq(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpgt_epi16(__m128i a, __m128i b) { return wasm_i16x8_gt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmplt_epi16(__m128i a, __m128i b) { return wasm_i16x8_lt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_adds_epi16(__m128i a, __m128i b) { return wasm_i16x8_add_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_adds_epu16(__m128i a, __m128i b) { return wasm_u16x8_add_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_subs_epi16(__m128i a, __m128i b) { return wasm_i16x8_sub_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_subs_epu16(__m128i a, __m128i b) { return wasm_u16x8_sub_sat(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_unpacklo_epi16(__m128i a,__m128i b) { return wasm_i16x8_shuffle(a,b,0,8,1,9,2,10,3,11); }
static inline __attribute__((always_inline)) __m128i _mm_unpackhi_epi16(__m128i a,__m128i b) { return wasm_i16x8_shuffle(a,b,4,12,5,13,6,14,7,15); }
static inline __attribute__((always_inline)) __m128i _mm_set1_epi32(int x) { return wasm_i32x4_splat(x); }
static inline __attribute__((always_inline)) __m128i _mm_set_epi32(int x0,int x1,int x2,int x3) { return wasm_i32x4_make(x3,x2,x1,x0); }
static inline __attribute__((always_inline)) __m128i _mm_setr_epi32(int x0,int x1,int x2,int x3) { return wasm_i32x4_make(x0,x1,x2,x3); }
static inline __attribute__((always_inline)) __m128i _mm_add_epi32(__m128i a, __m128i b) { return wasm_i32x4_add(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_sub_epi32(__m128i a, __m128i b) { return wasm_i32x4_sub(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpeq_epi32(__m128i a, __m128i b) { return wasm_i32x4_eq(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmpgt_epi32(__m128i a, __m128i b) { return wasm_i32x4_gt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_cmplt_epi32(__m128i a, __m128i b) { return wasm_i32x4_lt(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_unpacklo_epi32(__m128i a,__m128i b) { return wasm_i32x4_shuffle(a,b,0,4,1,5); }
static inline __attribute__((always_inline)) __m128i _mm_unpackhi_epi32(__m128i a,__m128i b) { return wasm_i32x4_shuffle(a,b,2,6,3,7); }
static inline __attribute__((always_inline)) __m128i _mm_set1_epi64x(long long x) { return wasm_i64x2_splat(x); }
static inline __attribute__((always_inline)) __m128i _mm_set_epi64x(long long x0,long long x1) { return wasm_i64x2_make(x1,x0); }
static inline __attribute__((always_inline)) __m128i _mm_setr_epi64x(long long x0,long long x1) { return wasm_i64x2_make(x0,x1); }
static inline __attribute__((always_inline)) __m128i _mm_add_epi64(__m128i a, __m128i b) { return wasm_i64x2_add(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_sub_epi64(__m128i a, __m128i b) { return wasm_i64x2_sub(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_unpacklo_epi64(__m128i a,__m128i b) { return wasm_i64x2_shuffle(a,b,0,2); }
static inline __attribute__((always_inline)) __m128i _mm_unpackhi_epi64(__m128i a,__m128i b) { return wasm_i64x2_shuffle(a,b,1,3); }
static inline __attribute__((always_inline)) __m128i _mm_setzero_si128(void) { return wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_undefined_si128(void) { return wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_and_si128(__m128i a, __m128i b) { return wasm_v128_and(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_or_si128(__m128i a, __m128i b) { return wasm_v128_or(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_xor_si128(__m128i a, __m128i b) { return wasm_v128_xor(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_andnot_si128(__m128i a,__m128i b) { return wasm_v128_andnot(b,a); }
static inline __attribute__((always_inline)) __m128i _mm_min_epu8(__m128i a, __m128i b) { return wasm_u8x16_min(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_max_epu8(__m128i a, __m128i b) { return wasm_u8x16_max(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_min_epi16(__m128i a, __m128i b) { return wasm_i16x8_min(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_max_epi16(__m128i a, __m128i b) { return wasm_i16x8_max(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_avg_epu8(__m128i a, __m128i b) { return wasm_u8x16_avgr(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_avg_epu16(__m128i a, __m128i b) { return wasm_u16x8_avgr(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_mullo_epi16(__m128i a, __m128i b) { return wasm_i16x8_mul(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_madd_epi16(__m128i a, __m128i b) { return wasm_i32x4_dot_i16x8(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_packs_epi16(__m128i a, __m128i b) { return wasm_i8x16_narrow_i16x8(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_packs_epi32(__m128i a, __m128i b) { return wasm_i16x8_narrow_i32x4(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_packus_epi16(__m128i a, __m128i b) { return wasm_u8x16_narrow_i16x8(a,b); }
static inline __attribute__((always_inline)) __m128i _mm_mulhi_epi16(__m128i a,__m128i b) { return wasm_i16x8_narrow_i32x4(wasm_i32x4_shr(wasm_i32x4_extmul_low_i16x8(a,b),16),wasm_i32x4_shr(wasm_i32x4_extmul_high_i16x8(a,b),16)); }
static inline __attribute__((always_inline)) __m128i _mm_mulhi_epu16(__m128i a,__m128i b) { return wasm_u16x8_narrow_i32x4(wasm_u32x4_shr(wasm_u32x4_extmul_low_u16x8(a,b),16),wasm_u32x4_shr(wasm_u32x4_extmul_high_u16x8(a,b),16)); }
static inline __attribute__((always_inline)) __m128i _mm_mul_epu32(__m128i a,__m128i b) { return wasm_u64x2_extmul_low_u32x4(wasm_i32x4_shuffle(a,a,0,2,0,2),wasm_i32x4_shuffle(b,b,0,2,0,2)); }
static inline __attribute__((always_inline)) __m128i _mm_slli_epi16(__m128i a,int count) { unsigned n=(unsigned)count;return n<16?wasm_i16x8_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_sll_epi16(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<16?wasm_i16x8_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srli_epi16(__m128i a,int count) { unsigned n=(unsigned)count;return n<16?wasm_u16x8_shr(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srl_epi16(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<16?wasm_u16x8_shr(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srai_epi16(__m128i a,int count) { unsigned n=(unsigned)count;return wasm_i16x8_shr(a,n<16?n:15); }
static inline __attribute__((always_inline)) __m128i _mm_sra_epi16(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return wasm_i16x8_shr(a,n<16?n:15); }
static inline __attribute__((always_inline)) __m128i _mm_slli_epi32(__m128i a,int count) { unsigned n=(unsigned)count;return n<32?wasm_i32x4_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_sll_epi32(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<32?wasm_i32x4_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srli_epi32(__m128i a,int count) { unsigned n=(unsigned)count;return n<32?wasm_u32x4_shr(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srl_epi32(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<32?wasm_u32x4_shr(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srai_epi32(__m128i a,int count) { unsigned n=(unsigned)count;return wasm_i32x4_shr(a,n<32?n:31); }
static inline __attribute__((always_inline)) __m128i _mm_sra_epi32(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return wasm_i32x4_shr(a,n<32?n:31); }
static inline __attribute__((always_inline)) __m128i _mm_slli_epi64(__m128i a,int count) { unsigned n=(unsigned)count;return n<64?wasm_i64x2_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_sll_epi64(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<64?wasm_i64x2_shl(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srli_epi64(__m128i a,int count) { unsigned n=(unsigned)count;return n<64?wasm_u64x2_shr(a,n):wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128i _mm_srl_epi64(__m128i a,__m128i count) { unsigned long long n=wasm_u64x2_extract_lane(count,0);return n<64?wasm_u64x2_shr(a,n):wasm_i32x4_const_splat(0); }
#define _mm_slli_si128(a,m) wasm_i8x16_shuffle((a),wasm_i32x4_const_splat(0),(((m)&255)>15||0<((m)&15)?16:0-((m)&15)),(((m)&255)>15||1<((m)&15)?16:1-((m)&15)),(((m)&255)>15||2<((m)&15)?16:2-((m)&15)),(((m)&255)>15||3<((m)&15)?16:3-((m)&15)),(((m)&255)>15||4<((m)&15)?16:4-((m)&15)),(((m)&255)>15||5<((m)&15)?16:5-((m)&15)),(((m)&255)>15||6<((m)&15)?16:6-((m)&15)),(((m)&255)>15||7<((m)&15)?16:7-((m)&15)),(((m)&255)>15||8<((m)&15)?16:8-((m)&15)),(((m)&255)>15||9<((m)&15)?16:9-((m)&15)),(((m)&255)>15||10<((m)&15)?16:10-((m)&15)),(((m)&255)>15||11<((m)&15)?16:11-((m)&15)),(((m)&255)>15||12<((m)&15)?16:12-((m)&15)),(((m)&255)>15||13<((m)&15)?16:13-((m)&15)),(((m)&255)>15||14<((m)&15)?16:14-((m)&15)),(((m)&255)>15||15<((m)&15)?16:15-((m)&15)))
#define _mm_bslli_si128 _mm_slli_si128
#define _mm_srli_si128(a,m) wasm_i8x16_shuffle((a),wasm_i32x4_const_splat(0),(((m)&255)>15?16:0+((m)&15)),(((m)&255)>15?16:1+((m)&15)),(((m)&255)>15?16:2+((m)&15)),(((m)&255)>15?16:3+((m)&15)),(((m)&255)>15?16:4+((m)&15)),(((m)&255)>15?16:5+((m)&15)),(((m)&255)>15?16:6+((m)&15)),(((m)&255)>15?16:7+((m)&15)),(((m)&255)>15?16:8+((m)&15)),(((m)&255)>15?16:9+((m)&15)),(((m)&255)>15?16:10+((m)&15)),(((m)&255)>15?16:11+((m)&15)),(((m)&255)>15?16:12+((m)&15)),(((m)&255)>15?16:13+((m)&15)),(((m)&255)>15?16:14+((m)&15)),(((m)&255)>15?16:15+((m)&15)))
#define _mm_bsrli_si128 _mm_srli_si128
#define _mm_extract_epi16(a,i) wasm_u16x8_extract_lane((a),((i)&7))
#define _mm_insert_epi16(a,x,i) wasm_i16x8_replace_lane((a),((i)&7),(x))
#define _mm_shuffle_epi32(a,m) __builtin(simd_shuffle_single,(a),((((m)>>0)&3)*4+0),((((m)>>0)&3)*4+1),((((m)>>0)&3)*4+2),((((m)>>0)&3)*4+3),((((m)>>2)&3)*4+0),((((m)>>2)&3)*4+1),((((m)>>2)&3)*4+2),((((m)>>2)&3)*4+3),((((m)>>4)&3)*4+0),((((m)>>4)&3)*4+1),((((m)>>4)&3)*4+2),((((m)>>4)&3)*4+3),((((m)>>6)&3)*4+0),((((m)>>6)&3)*4+1),((((m)>>6)&3)*4+2),((((m)>>6)&3)*4+3))
#define _mm_shufflelo_epi16(a,m) __builtin(simd_shuffle_single,(a),(((((m)>>0)&3)+0)*2+0),(((((m)>>0)&3)+0)*2+1),(((((m)>>2)&3)+0)*2+0),(((((m)>>2)&3)+0)*2+1),(((((m)>>4)&3)+0)*2+0),(((((m)>>4)&3)+0)*2+1),(((((m)>>6)&3)+0)*2+0),(((((m)>>6)&3)+0)*2+1),8,9,10,11,12,13,14,15)
#define _mm_shufflehi_epi16(a,m) __builtin(simd_shuffle_single,(a),0,1,2,3,4,5,6,7,(((((m)>>0)&3)+4)*2+0),(((((m)>>0)&3)+4)*2+1),(((((m)>>2)&3)+4)*2+0),(((((m)>>2)&3)+4)*2+1),(((((m)>>4)&3)+4)*2+0),(((((m)>>4)&3)+4)*2+1),(((((m)>>6)&3)+4)*2+0),(((((m)>>6)&3)+4)*2+1))
static inline __attribute__((always_inline)) __m128i _mm_load_si128(const __m128i *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) __m128i _mm_loadu_si128(const __m128i *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) void _mm_store_si128(__m128i *p,__m128i a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_storeu_si128(__m128i *p,__m128i a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_stream_si128(__m128i *p,__m128i a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) __m128i _mm_loadu_si32(const void *p) { return wasm_v128_load32_zero(p); }
static inline __attribute__((always_inline)) void _mm_storeu_si32(void *p,__m128i a) { wasm_v128_store32_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128i _mm_cvtsi32_si128(int x) { return wasm_i32x4_replace_lane(wasm_i32x4_const_splat(0),0,x); }
static inline __attribute__((always_inline)) int _mm_cvtsi128_si32(__m128i a) { return wasm_i32x4_extract_lane(a,0); }
static inline __attribute__((always_inline)) __m128i _mm_loadu_si64(const void *p) { return wasm_v128_load64_zero(p); }
static inline __attribute__((always_inline)) void _mm_storeu_si64(void *p,__m128i a) { wasm_v128_store64_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128i _mm_cvtsi64_si128(long long x) { return wasm_i64x2_replace_lane(wasm_i32x4_const_splat(0),0,x); }
static inline __attribute__((always_inline)) long long _mm_cvtsi128_si64(__m128i a) { return wasm_i64x2_extract_lane(a,0); }
static inline __attribute__((always_inline)) __m128i _mm_loadl_epi64(const __m128i *p) { return wasm_v128_load64_zero(p); }
static inline __attribute__((always_inline)) void _mm_storel_epi64(__m128i *p,__m128i a) { wasm_v128_store64_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128i _mm_move_epi64(__m128i a) { return wasm_i64x2_shuffle(a,wasm_i32x4_const_splat(0),0,2); }
static inline __attribute__((always_inline)) int _mm_movemask_epi8(__m128i a) { return wasm_i8x16_bitmask(a); }
static inline __attribute__((always_inline)) __m128i _mm_sad_epu8(__m128i a,__m128i b) { v128_t d=wasm_i8x16_sub(wasm_u8x16_max(a,b),wasm_u8x16_min(a,b));v128_t s=wasm_u32x4_extadd_pairwise_u16x8(wasm_u16x8_extadd_pairwise_u8x16(d));return wasm_i64x2_make(wasm_u32x4_extract_lane(s,0)+wasm_u32x4_extract_lane(s,1),wasm_u32x4_extract_lane(s,2)+wasm_u32x4_extract_lane(s,3)); }
static inline __attribute__((always_inline)) __m128d _mm_castps_pd(__m128 a) { return a; }
static inline __attribute__((always_inline)) __m128i _mm_castps_si128(__m128 a) { return a; }
static inline __attribute__((always_inline)) __m128 _mm_castpd_ps(__m128d a) { return a; }
static inline __attribute__((always_inline)) __m128i _mm_castpd_si128(__m128d a) { return a; }
static inline __attribute__((always_inline)) __m128 _mm_castsi128_ps(__m128i a) { return a; }
static inline __attribute__((always_inline)) __m128d _mm_castsi128_pd(__m128i a) { return a; }
#define _mm_cvtsi64x_si128 _mm_cvtsi64_si128
#define _mm_cvtsi128_si64x _mm_cvtsi128_si64
static inline __attribute__((always_inline)) __m128d _mm_cvtsi32_sd(__m128d a,int x) { return wasm_f64x2_replace_lane(a,0,(double)x); }
static inline __attribute__((always_inline)) int _mm_cvtsd_si32(__m128d a) { a=wasm_f64x2_nearest(a);double x=wasm_f64x2_extract_lane(a,0);return x>=-2147483648.0&&x<2147483648.0?(int)x:(-2147483647-1); }
static inline __attribute__((always_inline)) int _mm_cvttsd_si32(__m128d a) { a=wasm_f64x2_trunc(a);double x=wasm_f64x2_extract_lane(a,0);return x>=-2147483648.0&&x<2147483648.0?(int)x:(-2147483647-1); }
static inline __attribute__((always_inline)) __m128d _mm_cvtsi64_sd(__m128d a,long long x) { return wasm_f64x2_replace_lane(a,0,(double)x); }
static inline __attribute__((always_inline)) long long _mm_cvtsd_si64(__m128d a) { a=wasm_f64x2_nearest(a);double x=wasm_f64x2_extract_lane(a,0);return x>=-9223372036854775808.0&&x<9223372036854775808.0?(long long)x:(-9223372036854775807LL-1); }
static inline __attribute__((always_inline)) long long _mm_cvttsd_si64(__m128d a) { a=wasm_f64x2_trunc(a);double x=wasm_f64x2_extract_lane(a,0);return x>=-9223372036854775808.0&&x<9223372036854775808.0?(long long)x:(-9223372036854775807LL-1); }
static inline __attribute__((always_inline)) int _mm_comieq_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) == wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomieq_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) == wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comineq_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) != wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomineq_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) != wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comilt_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) < wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomilt_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) < wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comile_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) <= wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomile_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) <= wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comigt_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) > wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomigt_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) > wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comige_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) >= wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomige_sd(__m128d a,__m128d b) { return wasm_f64x2_extract_lane(a,0) >= wasm_f64x2_extract_lane(b,0); }
static inline __attribute__((always_inline)) __m128d _mm_loadr_pd(const double *p) { v128_t a=wasm_v128_load(p);return wasm_i64x2_shuffle(a,a,1,0); }
static inline __attribute__((always_inline)) void _mm_storer_pd(double *p,__m128d a) { wasm_v128_store(p,wasm_i64x2_shuffle(a,a,1,0)); }
static inline __attribute__((always_inline)) __m128d _mm_loadl_pd(__m128d a,const double *p) { return wasm_v128_load64_lane(p,a,0); }
static inline __attribute__((always_inline)) void _mm_storel_pd(double *p,__m128d a) { wasm_v128_store64_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128d _mm_loadh_pd(__m128d a,const double *p) { return wasm_v128_load64_lane(p,a,1); }
static inline __attribute__((always_inline)) void _mm_storeh_pd(double *p,__m128d a) { wasm_v128_store64_lane(p,a,1); }
static inline __attribute__((always_inline)) __m128 _mm_cvtpd_ps(__m128d a) { return wasm_f32x4_demote_f64x2_zero(a); }
static inline __attribute__((always_inline)) __m128d _mm_cvtps_pd(__m128 a) { return wasm_f64x2_promote_low_f32x4(a); }
static inline __attribute__((always_inline)) __m128d _mm_cvtepi32_pd(__m128i a) { return wasm_f64x2_convert_low_i32x4(a); }
static inline __attribute__((always_inline)) __m128 _mm_cvtepi32_ps(__m128i a) { return wasm_f32x4_convert_i32x4(a); }
static inline __attribute__((always_inline)) __m128 _mm_cvtsd_ss(__m128 a,__m128d b) { return wasm_f32x4_replace_lane(a,0,(float)wasm_f64x2_extract_lane(b,0)); }
static inline __attribute__((always_inline)) __m128d _mm_cvtss_sd(__m128d a,__m128 b) { return wasm_f64x2_replace_lane(a,0,(double)wasm_f32x4_extract_lane(b,0)); }
static inline __attribute__((always_inline)) __m128i _mm_cvttps_epi32(__m128 a) { a=wasm_f32x4_trunc(a);v128_t valid=wasm_v128_and(wasm_f32x4_ge(a,wasm_f32x4_splat(-2147483648.0f)),wasm_f32x4_lt(a,wasm_f32x4_splat(2147483648.0f)));return wasm_v128_bitselect(wasm_i32x4_trunc_sat_f32x4(a),wasm_i32x4_const_splat(-2147483647-1),valid); }
static inline __attribute__((always_inline)) __m128i _mm_cvtps_epi32(__m128 a) { return _mm_cvttps_epi32(wasm_f32x4_nearest(a)); }
static inline __attribute__((always_inline)) __m128i _mm_cvttpd_epi32(__m128d a) { return wasm_i32x4_make(_mm_cvttsd_si32(a),_mm_cvttsd_si32(wasm_i64x2_shuffle(a,a,1,0)),0,0); }
static inline __attribute__((always_inline)) __m128i _mm_cvtpd_epi32(__m128d a) { return _mm_cvttpd_epi32(wasm_f64x2_nearest(a)); }
static inline __attribute__((always_inline)) __m128i _mm_loadu_si16(const void *p) { return wasm_v128_load16_lane(p,wasm_i32x4_const_splat(0),0); }
static inline __attribute__((always_inline)) void _mm_storeu_si16(void *p,__m128i a) { wasm_v128_store16_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128i _mm_set_epi64(__m64 hi,__m64 lo) { return wasm_i64x2_make(lo,hi); }
static inline __attribute__((always_inline)) __m128i _mm_setr_epi64(__m64 lo,__m64 hi) { return wasm_i64x2_make(lo,hi); }
static inline __attribute__((always_inline)) __m128i _mm_set1_epi64(__m64 a) { return wasm_i64x2_splat(a); }
static inline __attribute__((always_inline)) void _mm_maskmoveu_si128(__m128i a,__m128i mask,char *p) { unsigned char values[16],masks[16];wasm_v128_store(values,a);wasm_v128_store(masks,mask);for(int i=0;i<16;i++)if(masks[i]&128)p[i]=values[i]; }
static inline __attribute__((always_inline)) void _mm_stream_si32(int *p,int a) { *p=a; }
static inline __attribute__((always_inline)) void _mm_stream_si64(long long *p,long long a) { *p=a; }
static inline __attribute__((always_inline)) void _mm_lfence(void) { __builtin(atomic_fence); }
static inline __attribute__((always_inline)) void _mm_mfence(void) { __builtin(atomic_fence); }
static inline __attribute__((always_inline)) void _mm_clflush(const void *p) { (void)p; }
static inline __attribute__((always_inline)) void _mm_pause(void) {  }
#define _mm_cvtsd_si64x _mm_cvtsd_si64
#define _mm_cvttsd_si64x _mm_cvttsd_si64
#define _mm_cvtsi64x_sd _mm_cvtsi64_sd
#endif
