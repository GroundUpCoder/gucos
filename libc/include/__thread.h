
#pragma once
/* Thread runtime record (docs/MEMORY-ABI.md). The first 32 bytes and the
   exception payload belong to the trampoline; libc fields start at 2048.
   Frames start at record + 3072. */
#define __THREAD_LIMIT     28      /* top of this thread's C stack (trampoline) */
#define __THREAD_STATE     2048    /* 0 running, 1 exited (atomic) */
#define __THREAD_DETACHED  2052
#define __THREAD_RETVAL    2056
#define __THREAD_ERRNO     2060
#define __THREAD_ARENA     2064    /* arena size in bytes */
#define __THREAD_FN        2068
#define __THREAD_ARG       2072
#define __THREAD_TSD       2076    /* per-thread key values, allocated lazily */
#define __THREAD_TID       2080
#define __THREAD_RAW       2084    /* malloc pointer that owns the arena */
#define __THREAD_EXITBUF   2088    /* jmp_buf for pthread_exit */
#define __THREAD_NEXT      2092    /* all-threads list */
#define __THREAD_MAIN      1024
#define __THREAD_LIVE      (__THREAD_MAIN + 2096)  /* process-wide live thread count */
#define __THREAD_LOCKED    (__THREAD_MAIN + 2100)  /* all-threads list lock */
#define __THREAD_TLS       2104    /* TLS block address, owned by the same allocation */
#define __THREAD_SIGBLOCK  2108    /* inherited per-thread signal mask */
#define __THREAD_SIGPENDING 2112   /* thread-directed pending signals */
#define __THREAD_PROCESS_SIGNALS 2116 /* private clone signal table, or zero */
#define __THREAD_PRIVATE_UMASK 2120
#define __THREAD_UMASK_VALUE 2124
#define __THREAD_RECORD_BYTES 3072
#define __THREAD_FIELD(rec, off) (*(volatile unsigned *)((unsigned)(rec) + (off)))

/* Recursive lock on a 32-bit word triple, futex style. Uncontended in a
   single-threaded program, so it never reaches memory.atomic.wait there. */
typedef struct { int lock; unsigned owner; int count; } __lock_t;
#define __LOCK_INIT {0, 0, 0}
__import int __thread_wait(int *address, int expected, long long timeout);
static inline void __lock_acquire(__lock_t *l) {
  unsigned self = __builtin(thread_record);
  if (l->lock && l->owner == self) { l->count++; return; }
  int c = __builtin(atomic_cmpxchg32, &l->lock, 0, 1);
  if (c) {
    if (c != 2) c = __builtin(atomic_xchg32, &l->lock, 2);
    while (c) { __thread_wait(&l->lock, 2, -1LL); c = __builtin(atomic_xchg32, &l->lock, 2); }
  }
  l->owner = self; l->count = 1;
}
static inline void __lock_release(__lock_t *l) {
  if (--l->count > 0) return;
  l->owner = 0;
  if (__builtin(atomic_sub32, &l->lock, 1) != 1) { __builtin(atomic_store32, &l->lock, 0); __builtin(atomic_notify, &l->lock, 1); }
}
  