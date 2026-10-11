
#pragma once
/* Browser workers are scheduled by the browser. sched_yield is an advisory
   no-op; blocking synchronization uses kernel RPCs. Only the ordinary
   scheduling policy is supported, not real-time priorities. */
#include <signal.h>
#define CLONE_VM 0x00000100
#define CLONE_FS 0x00000200
#define CLONE_FILES 0x00000400
#define CLONE_SIGHAND 0x00000800
#define CLONE_VFORK 0x00004000
#define CLONE_THREAD 0x00010000
/* This runtime reserves the bottom 3072 bytes for continuation bookkeeping.
   Supply at least this many writable bytes below the aligned stack top. */
#define WASM_POSIX_CLONE_STACK_MIN 1048576
__require_source("__clone.c");
/* No parameter names: application headers (gawk's awk.h) define macros such
   as stack that would otherwise rewrite this prototype. */
int clone(int (*)(void *), void *, int, void *, ...);
struct sched_param { int sched_priority; };
#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
static inline int sched_yield(void) { return 0; }
