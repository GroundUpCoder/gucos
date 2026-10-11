
#pragma once
#include <sys/types.h>
#include <sys/time.h>
typedef unsigned long long rlim_t;
#define RLIM_INFINITY  (~0ULL)
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY
#define RLIMIT_CPU     0
#define RLIMIT_FSIZE   1
#define RLIMIT_DATA    2
#define RLIMIT_STACK   3
#define RLIMIT_CORE    4
#define RLIMIT_RSS     5
#define RLIMIT_NPROC   6
#define RLIMIT_NOFILE  7
#define RLIMIT_MEMLOCK 8
#define RLIMIT_AS      9
#define RLIM_NLIMITS   10
struct rlimit { rlim_t rlim_cur; rlim_t rlim_max; };
/* No resource limits are enforced in this runtime: report "unlimited" and
   accept any setrlimit as a no-op. */
static inline int getrlimit(int resource, struct rlimit *rlim) {
  (void)resource;
  if (rlim) { rlim->rlim_cur = RLIM_INFINITY; rlim->rlim_max = RLIM_INFINITY; }
  return 0;
}
static inline int setrlimit(int resource, const struct rlimit *rlim) {
  (void)resource; (void)rlim;
  return 0;
}
#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN (-1)
struct rusage {
  struct timeval ru_utime;
  struct timeval ru_stime;
  long ru_maxrss; long ru_ixrss; long ru_idrss; long ru_isrss;
  long ru_minflt; long ru_majflt; long ru_nswap;
  long ru_inblock; long ru_oublock;
  long ru_msgsnd; long ru_msgrcv; long ru_nsignals;
  long ru_nvcsw; long ru_nivcsw;
};
/* Resource usage isn't tracked: zero everything. */
static inline int getrusage(int who, struct rusage *usage) {
  (void)who;
  if (usage) { char *p = (char *)usage; for (unsigned long i = 0; i < sizeof(*usage); i++) p[i] = 0; }
  return 0;
}
  