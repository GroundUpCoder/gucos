
#pragma once
__require_source("__posix.c");   /* futimesat */
#include <time.h>
struct timeval {
  time_t tv_sec;   /* 64-bit (matches struct stat's st_*tim); range past 2038 */
  long tv_usec;
};
/* BSD timeval comparison; like the system macro, arguments may be evaluated
   more than once. Compare seconds first, then fractional seconds. */
#define timercmp(a,b,op) (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec op (b)->tv_usec) : ((a)->tv_sec op (b)->tv_sec))
__import int __gettimeofday(long long *sec, long *usec);
/* Host primitives that actually set a file's access/modification times
   (whole seconds). Faithful on the Node and BlockFS backends; the OPFS
   backend has no timestamp API and treats them as existence-checked no-ops. */
__import int __utime(const char *path, long long atime, long long mtime);
__import int __futime(int fd, long long atime, long long mtime);
/* futimesat (legacy, docs/archive/0325 Group B — posixmodule.c). Superseded by
   utimensat, but still referenced. A NULL times means "now". */
int futimesat(int dirfd, const char *path, const struct timeval times[2]);
int gettimeofday(struct timeval *tv, void *tz);
/* utimes()/futimes(): set access+modification times. A NULL `times` means
   "now". POSIX timeval is microsecond-resolution, but this environment's
   filesystems store whole-second times, so the sub-second part is truncated
   (POSIX permits coarser filesystem granularity). Errors (e.g. ENOENT for a
   missing path) come back through errno from the host. */
static inline int utimes(const char *path, const struct timeval times[2]) {
  time_t a, m;
  if (times == 0) { struct timeval now; gettimeofday(&now, 0); a = m = now.tv_sec; }
  else { a = times[0].tv_sec; m = times[1].tv_sec; }
  return __utime(path, a, m);
}
static inline int futimes(int fd, const struct timeval times[2]) {
  time_t a, m;
  if (times == 0) { struct timeval now; gettimeofday(&now, 0); a = m = now.tv_sec; }
  else { a = times[0].tv_sec; m = times[1].tv_sec; }
  return __futime(fd, a, m);
}
/* ---- interval timers (docs/archive/0044) ----
   ONE kernel-side real-time timer per process; expiry posts SIGALRM through
   the ordinary cooperative signal path (safe points — the settled 0001
   caveat applies). ITIMER_VIRTUAL/PROF fail with EINVAL: workers run on
   their own OS threads, so there is no CPU accounting to back them.
   Millisecond resolution (sub-ms rounds UP so a tiny-but-armed timer never
   silently becomes "disarmed"). Implementations live in __signal.c, which
   links into every stdlib program via abort(). */
#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2
struct itimerval {
  struct timeval it_interval;   /* reload value; zero = one-shot */
  struct timeval it_value;      /* time to expiry; zero = disarmed */
};
int setitimer(int __which, const struct itimerval *__nv, struct itimerval *__ov);
int getitimer(int __which, struct itimerval *__cur);
#include <sys/select.h>  // glibc-style: fd_set / FD_* live here under _GNU_SOURCE
  