
#pragma once
#include <sys/time.h>   /* __utime + gettimeofday */
struct utimbuf { time_t actime; time_t modtime; };
/* Legacy POSIX utime(). A NULL times argument means "now". */
static inline int utime(const char *path, const struct utimbuf *times) {
  if (times == 0) { struct timeval now; gettimeofday(&now, 0); return __utime(path, now.tv_sec, now.tv_sec); }
  return __utime(path, times->actime, times->modtime);
}
  