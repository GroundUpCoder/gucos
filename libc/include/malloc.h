
#pragma once
/* glibc places the malloc family in <malloc.h> too; forward to <stdlib.h>. */
#include <stdlib.h>
#define M_TRIM_THRESHOLD    -1
#define M_TOP_PAD           -2
#define M_MMAP_THRESHOLD    -3
#define M_MMAP_MAX          -4
#define M_CHECK_ACTION      -5
#define M_PERTURB           -6
#define M_ARENA_TEST        -7
#define M_ARENA_MAX         -8
/* This allocator exposes no tuning or usage introspection: mallopt() accepts
   any option (returns success), malloc_trim() frees nothing, and
   malloc_usable_size() reports 0 (the real slack isn't tracked). */
static inline int mallopt(int param, int value) { (void)param; (void)value; return 1; }
static inline int malloc_trim(unsigned long pad) { (void)pad; return 0; }
static inline unsigned long malloc_usable_size(void *ptr) { (void)ptr; return 0; }
  