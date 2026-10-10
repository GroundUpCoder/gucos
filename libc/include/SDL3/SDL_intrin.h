
#pragma once
#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__SSE__)
#include <xmmintrin.h>
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif
