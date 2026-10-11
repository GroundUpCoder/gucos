#ifndef _XMMINTRIN_H
#define _XMMINTRIN_H
#ifndef __SSE__
#error this header requires -msse
#endif
#include <wasm_simd128.h>
typedef v128_t __m128;
typedef v128_t __m128i;
#define _MM_SHUFFLE(z,y,x,w) (((z)<<6)|((y)<<4)|((x)<<2)|(w))
static inline __attribute__((always_inline)) __m128 _mm_setzero_ps(void) { return wasm_i32x4_const_splat(0); }
static inline __attribute__((always_inline)) __m128 _mm_set1_ps(float x) { return wasm_f32x4_splat(x); }
#define _mm_set_ps1 _mm_set1_ps
static inline __attribute__((always_inline)) __m128 _mm_set_ps(float a,float b,float c,float d) { return wasm_f32x4_make(d,c,b,a); }
static inline __attribute__((always_inline)) __m128 _mm_setr_ps(float a,float b,float c,float d) { return wasm_f32x4_make(a,b,c,d); }
static inline __attribute__((always_inline)) __m128 _mm_set_ss(float x) { return wasm_f32x4_replace_lane(wasm_i32x4_const_splat(0),0,x); }
static inline __attribute__((always_inline)) __m128 _mm_move_ss(__m128 a,__m128 b) { return wasm_f32x4_replace_lane(a,0,wasm_f32x4_extract_lane(b,0)); }
static inline __attribute__((always_inline)) __m128 _mm_add_ps(__m128 a, __m128 b) { return wasm_f32x4_add(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_add_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_add_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_sub_ps(__m128 a, __m128 b) { return wasm_f32x4_sub(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_sub_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_sub_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_mul_ps(__m128 a, __m128 b) { return wasm_f32x4_mul(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_mul_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_mul_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_div_ps(__m128 a, __m128 b) { return wasm_f32x4_div(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_div_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_div_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_sqrt_ps(__m128 a) { return wasm_f32x4_sqrt(a); }
static inline __attribute__((always_inline)) __m128 _mm_sqrt_ss(__m128 a) { return _mm_move_ss(a,_mm_sqrt_ps(a)); }
static inline __attribute__((always_inline)) __m128 _mm_min_ps(__m128 a,__m128 b) { return wasm_v128_bitselect(a,b,wasm_f32x4_lt(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_min_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_min_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_max_ps(__m128 a,__m128 b) { return wasm_v128_bitselect(a,b,wasm_f32x4_gt(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_max_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_max_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_and_ps(__m128 a, __m128 b) { return wasm_v128_and(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_or_ps(__m128 a, __m128 b) { return wasm_v128_or(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_xor_ps(__m128 a, __m128 b) { return wasm_v128_xor(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_andnot_ps(__m128 a,__m128 b) { return wasm_v128_andnot(b,a); }
static inline __attribute__((always_inline)) __m128 _mm_cmpeq_ps(__m128 a,__m128 b) { return wasm_f32x4_eq(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmpeq_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpeq_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmplt_ps(__m128 a,__m128 b) { return wasm_f32x4_lt(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmplt_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmplt_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmple_ps(__m128 a,__m128 b) { return wasm_f32x4_le(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmple_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmple_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpgt_ps(__m128 a,__m128 b) { return wasm_f32x4_gt(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmpgt_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpgt_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpge_ps(__m128 a,__m128 b) { return wasm_f32x4_ge(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmpge_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpge_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpneq_ps(__m128 a,__m128 b) { return wasm_f32x4_ne(a,b); }
static inline __attribute__((always_inline)) __m128 _mm_cmpneq_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpneq_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnlt_ps(__m128 a,__m128 b) { return wasm_v128_not(wasm_f32x4_lt(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnlt_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpnlt_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnle_ps(__m128 a,__m128 b) { return wasm_v128_not(wasm_f32x4_le(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnle_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpnle_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpngt_ps(__m128 a,__m128 b) { return wasm_v128_not(wasm_f32x4_gt(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpngt_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpngt_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnge_ps(__m128 a,__m128 b) { return wasm_v128_not(wasm_f32x4_ge(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpnge_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpnge_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpord_ps(__m128 a,__m128 b) { return wasm_v128_and(wasm_f32x4_eq(a,a),wasm_f32x4_eq(b,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpunord_ps(__m128 a,__m128 b) { return wasm_v128_not(_mm_cmpord_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpord_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpord_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_cmpunord_ss(__m128 a,__m128 b) { return _mm_move_ss(a,_mm_cmpunord_ps(a,b)); }
static inline __attribute__((always_inline)) __m128 _mm_loadu_ps(const float *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) __m128 _mm_load_ps(const float *p) { return wasm_v128_load(p); }
static inline __attribute__((always_inline)) __m128 _mm_load_ss(const float *p) { return wasm_v128_load32_zero(p); }
static inline __attribute__((always_inline)) __m128 _mm_load1_ps(const float *p) { return wasm_v128_load32_splat(p); }
#define _mm_load_ps1 _mm_load1_ps
static inline __attribute__((always_inline)) void _mm_store_ps(float *p,__m128 a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_storeu_ps(float *p,__m128 a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_stream_ps(float *p,__m128 a) { wasm_v128_store(p,a); }
static inline __attribute__((always_inline)) void _mm_store_ss(float *p,__m128 a) { wasm_v128_store32_lane(p,a,0); }
static inline __attribute__((always_inline)) void _mm_store1_ps(float *p,__m128 a) { wasm_v128_store(p,wasm_f32x4_splat(wasm_f32x4_extract_lane(a,0))); }
#define _mm_store_ps1 _mm_store1_ps
static inline __attribute__((always_inline)) int _mm_movemask_ps(__m128 a) { return wasm_i32x4_bitmask(a); }
static inline __attribute__((always_inline)) float _mm_cvtss_f32(__m128 a) { return wasm_f32x4_extract_lane(a,0); }
static inline __attribute__((always_inline)) __m128 _mm_unpacklo_ps(__m128 a,__m128 b) { return wasm_i32x4_shuffle(a,b,0,4,1,5); }
static inline __attribute__((always_inline)) __m128 _mm_unpackhi_ps(__m128 a,__m128 b) { return wasm_i32x4_shuffle(a,b,2,6,3,7); }
static inline __attribute__((always_inline)) __m128 _mm_undefined_ps(void) { return wasm_i32x4_const_splat(0); }
#define _mm_shuffle_ps(a,b,m) wasm_i32x4_shuffle((a),(b),((m)&3),(((m)>>2)&3),((((m)>>4)&3)+4),((((m)>>6)&3)+4))
static inline __attribute__((always_inline)) __m128 _mm_movehl_ps(__m128 a,__m128 b) { return wasm_i32x4_shuffle(b,a,2,3,6,7); }
static inline __attribute__((always_inline)) __m128 _mm_movelh_ps(__m128 a,__m128 b) { return wasm_i32x4_shuffle(a,b,0,1,4,5); }
static inline __attribute__((always_inline)) __m128 _mm_rcp_ps(__m128 a) { return wasm_f32x4_div(wasm_f32x4_splat(1.0f),a); }
static inline __attribute__((always_inline)) __m128 _mm_rsqrt_ps(__m128 a) { return wasm_f32x4_div(wasm_f32x4_splat(1.0f),wasm_f32x4_sqrt(a)); }
static inline __attribute__((always_inline)) __m128 _mm_rcp_ss(__m128 a) { return _mm_move_ss(a,_mm_rcp_ps(a)); }
static inline __attribute__((always_inline)) __m128 _mm_rsqrt_ss(__m128 a) { return _mm_move_ss(a,_mm_rsqrt_ps(a)); }
static inline __attribute__((always_inline)) __m128 _mm_cvtsi32_ss(__m128 a,int x) { return wasm_f32x4_replace_lane(a,0,(float)x); }
static inline __attribute__((always_inline)) int _mm_cvtss_si32(__m128 a) { a=wasm_f32x4_nearest(a);float x=wasm_f32x4_extract_lane(a,0);return x>=-2147483648.0&&x<2147483648.0?(int)x:(-2147483647-1); }
static inline __attribute__((always_inline)) int _mm_cvttss_si32(__m128 a) { a=wasm_f32x4_trunc(a);float x=wasm_f32x4_extract_lane(a,0);return x>=-2147483648.0&&x<2147483648.0?(int)x:(-2147483647-1); }
static inline __attribute__((always_inline)) __m128 _mm_cvtsi64_ss(__m128 a,long long x) { return wasm_f32x4_replace_lane(a,0,(float)x); }
static inline __attribute__((always_inline)) long long _mm_cvtss_si64(__m128 a) { a=wasm_f32x4_nearest(a);float x=wasm_f32x4_extract_lane(a,0);return x>=-9223372036854775808.0&&x<9223372036854775808.0?(long long)x:(-9223372036854775807LL-1); }
static inline __attribute__((always_inline)) long long _mm_cvttss_si64(__m128 a) { a=wasm_f32x4_trunc(a);float x=wasm_f32x4_extract_lane(a,0);return x>=-9223372036854775808.0&&x<9223372036854775808.0?(long long)x:(-9223372036854775807LL-1); }
static inline __attribute__((always_inline)) int _mm_comieq_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) == wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomieq_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) == wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comineq_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) != wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomineq_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) != wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comilt_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) < wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomilt_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) < wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comile_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) <= wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomile_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) <= wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comigt_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) > wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomigt_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) > wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_comige_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) >= wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) int _mm_ucomige_ss(__m128 a,__m128 b) { return wasm_f32x4_extract_lane(a,0) >= wasm_f32x4_extract_lane(b,0); }
static inline __attribute__((always_inline)) __m128 _mm_loadr_ps(const float *p) { v128_t a=wasm_v128_load(p);return wasm_i32x4_shuffle(a,a,3,2,1,0); }
static inline __attribute__((always_inline)) void _mm_storer_ps(float *p,__m128 a) { wasm_v128_store(p,wasm_i32x4_shuffle(a,a,3,2,1,0)); }
typedef long long __m64;
#define _MM_HINT_NTA 0
#define _MM_HINT_T2 1
#define _MM_HINT_T1 2
#define _MM_HINT_T0 3
static inline __attribute__((always_inline)) __m128 _mm_loadl_pi(__m128 a,const __m64 *p) { return wasm_v128_load64_lane(p,a,0); }
static inline __attribute__((always_inline)) void _mm_storel_pi(__m64 *p,__m128 a) { wasm_v128_store64_lane(p,a,0); }
static inline __attribute__((always_inline)) __m128 _mm_loadh_pi(__m128 a,const __m64 *p) { return wasm_v128_load64_lane(p,a,1); }
static inline __attribute__((always_inline)) void _mm_storeh_pi(__m64 *p,__m128 a) { wasm_v128_store64_lane(p,a,1); }
static inline __attribute__((always_inline)) void _mm_prefetch(const char *p,int hint) { (void)p;(void)hint; }
static inline __attribute__((always_inline)) void _mm_sfence(void) { __builtin(atomic_fence); }
#define _mm_cvt_si2ss _mm_cvtsi32_ss
#define _mm_cvt_ss2si _mm_cvtss_si32
#define _mm_cvtt_ss2si _mm_cvttss_si32
#define _mm_undefined _mm_undefined_ps
#include <__malloc.h>
static inline __attribute__((always_inline)) void * _mm_malloc(size_t size,size_t alignment) { if(!alignment||(alignment&(alignment-1)))return (void*)0;if(alignment<sizeof(void*))alignment=sizeof(void*);if(size>(size_t)-1-alignment-sizeof(void*))return (void*)0;void *base=malloc(size+alignment+sizeof(void*));if(!base)return (void*)0;void *p=(void*)(((uintptr_t)base+sizeof(void*)+alignment-1)&~(alignment-1));((void**)p)[-1]=base;return p; }
static inline __attribute__((always_inline)) void _mm_free(void *p) { if(p)free(((void**)p)[-1]); }
#endif
