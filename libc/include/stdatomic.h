
#pragma once
/* C11 atomics over the 32-bit Wasm atomic intrinsics (docs/THREADS.md). Only
   4-byte objects are atomic; wider objects fall back to plain accesses. */
#include <stddef.h>
#include <stdint.h>
#define _Atomic(T) T
typedef enum { memory_order_relaxed, memory_order_consume, memory_order_acquire, memory_order_release, memory_order_acq_rel, memory_order_seq_cst } memory_order;
typedef int atomic_int; typedef unsigned atomic_uint; typedef int atomic_bool; typedef long atomic_long; typedef unsigned long atomic_ulong;
typedef int32_t atomic_int32_t; typedef uint32_t atomic_uint32_t; typedef size_t atomic_size_t; typedef intptr_t atomic_intptr_t; typedef uintptr_t atomic_uintptr_t;
#define ATOMIC_VAR_INIT(v) (v)
#define ATOMIC_FLAG_INIT {0}
#define atomic_init(p, v) (*(p) = (v))
#define atomic_is_lock_free(p) (sizeof(*(p)) == 4)
#define atomic_thread_fence(o) __builtin(atomic_fence)
#define atomic_signal_fence(o) __builtin(atomic_fence)
#define atomic_load(p) (sizeof(*(p)) == 4 ? (__typeof__(*(p)))__builtin(atomic_load32, (int *)(p)) : *(p))
#define atomic_load_explicit(p, o) atomic_load(p)
#define atomic_store(p, v) ((void)(sizeof(*(p)) == 4 ? (__builtin(atomic_store32, (int *)(p), (int)(v)), 0) : (*(p) = (v), 0)))
#define atomic_store_explicit(p, v, o) atomic_store(p, v)
#define atomic_exchange(p, v) ((__typeof__(*(p)))__builtin(atomic_xchg32, (int *)(p), (int)(v)))
#define atomic_exchange_explicit(p, v, o) atomic_exchange(p, v)
#define atomic_fetch_add(p, v) ((__typeof__(*(p)))__builtin(atomic_add32, (int *)(p), (int)(v)))
#define atomic_fetch_add_explicit(p, v, o) atomic_fetch_add(p, v)
#define atomic_fetch_sub(p, v) ((__typeof__(*(p)))__builtin(atomic_sub32, (int *)(p), (int)(v)))
#define atomic_fetch_sub_explicit(p, v, o) atomic_fetch_sub(p, v)
#define atomic_fetch_and(p, v) ((__typeof__(*(p)))__builtin(atomic_and32, (int *)(p), (int)(v)))
#define atomic_fetch_or(p, v) ((__typeof__(*(p)))__builtin(atomic_or32, (int *)(p), (int)(v)))
#define atomic_fetch_xor(p, v) ((__typeof__(*(p)))__builtin(atomic_xor32, (int *)(p), (int)(v)))
static inline int __atomic_cas32(int *p, int *expected, int desired) {
  int seen = __builtin(atomic_cmpxchg32, p, *expected, desired);
  if (seen == *expected) return 1;
  *expected = seen; return 0;
}
#define atomic_compare_exchange_strong(p, e, d) __atomic_cas32((int *)(p), (int *)(e), (int)(d))
#define atomic_compare_exchange_weak(p, e, d) __atomic_cas32((int *)(p), (int *)(e), (int)(d))
#define atomic_compare_exchange_strong_explicit(p, e, d, o1, o2) atomic_compare_exchange_strong(p, e, d)
#define atomic_compare_exchange_weak_explicit(p, e, d, o1, o2) atomic_compare_exchange_weak(p, e, d)
typedef struct { int _v; } atomic_flag;
#define atomic_flag_test_and_set(f) (__builtin(atomic_xchg32, &(f)->_v, 1) != 0)
#define atomic_flag_clear(f) __builtin(atomic_store32, &(f)->_v, 0)
  