
#pragma once
/* struct timespec + clockid_t, factored out so BOTH <time.h> and
   <sys/types.h> can expose them from ONE definition (docs/archive/0325 Group A).
 *
 * CPython — like musl and wasi-libc — expects these visible via
 * <sys/types.h> before it reaches Include/cpython/pthread_stubs.h, and the
 * symptom of their absence used to be a baffling "type specifier missing"
 * pointing at pthread_stubs.h:78; since #708 it names the missing type.
 *
 * It is a separate header, rather than <sys/types.h> including <time.h>,
 * because <time.h> carries __require_source("__time.c") and <sys/types.h>
 * is included by nearly everything — that route would link the whole time
 * TU into every binary that names a size_t.
 */
typedef long long time_t;

struct timespec {
  long long tv_sec;   // 64-bit (matches struct stat's st_*tim); range past 2038
  long      tv_nsec;
};

typedef int clockid_t;
  