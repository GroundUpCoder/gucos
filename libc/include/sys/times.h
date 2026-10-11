
#pragma once
#include <sys/types.h>
#include <time.h>
struct tms {
  clock_t tms_utime;
  clock_t tms_stime;
  clock_t tms_cutime;
  clock_t tms_cstime;
};
/* No per-process CPU accounting: report all elapsed time as user time of the
   single process (children are always 0), consistent with clock(). */
static inline clock_t times(struct tms *buf) {
  clock_t c = clock();
  if (buf) { buf->tms_utime = c; buf->tms_stime = 0; buf->tms_cutime = 0; buf->tms_cstime = 0; }
  return c;
}
  