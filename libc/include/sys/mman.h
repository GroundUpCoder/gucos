
#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED    ((void *)-1)
#define MS_ASYNC      1
#define MS_SYNC       4
#define MS_INVALIDATE 2
/* Efficient subset: private read/write snapshots in ordinary linear memory.
   Shared mappings, page protection and address replacement fail with ENOTSUP.
   See docs/MAPPINGS.md. */
void *mmap(void *, size_t, int, int, int, long long);
int munmap(void *, size_t);
int mprotect(void *, size_t, int);
int msync(void *, size_t, int);
__require_source("__mman.c");
  