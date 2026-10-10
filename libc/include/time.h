
#pragma once
__require_source("__time.c");
#include <stddef.h>

#include <__timespec.h>   /* time_t, struct timespec, clockid_t — one definition */
typedef long clock_t;

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  long tm_gmtoff;
  /* tm_zone (BSD/glibc extension, docs/archive/0325 Group B). Shipping tm_gmtoff
     without it was the surprising half: CPython's timemodule tests for
     tm_zone and, not finding it, falls back to tzset()+strftime. Points at
     static storage, as on glibc — do not free it. */
  const char *tm_zone;
};

#define CLOCKS_PER_SEC 1000000
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

time_t time(time_t *t);
clock_t clock(void);
double difftime(time_t t1, time_t t0);
struct tm *gmtime(const time_t *timep);
/* gmtime_r: owned by docs/archive/0325 Group A (docs/archive/0382 gap 4 defers to it). */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime(const time_t *timep);
struct tm *localtime_r(const time_t *timep, struct tm *result);
time_t mktime(struct tm *tm);
char *asctime(const struct tm *tm);
char *ctime(const time_t *timep);
/* Reentrant twins; POSIX requires buf to hold at least 26 bytes. */
char *asctime_r(const struct tm *tm, char *buf);
char *ctime_r(const time_t *timep, char *buf);
size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);
/* strptime (POSIX XSI, + the glibc %F/%s/%z extensions): ticket #113. */
char *strptime(const char *s, const char *format, struct tm *tm);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_getres(clockid_t clk_id, struct timespec *res);
/* timegm: mktime's UTC twin (docs/archive/0382 gap 5, docs/archive/0325 Group B). */
time_t timegm(struct tm *tm);
/* clock_nanosleep: RETURNS an error number, does not set errno. */
#define TIMER_ABSTIME 1
int clock_nanosleep(clockid_t clk_id, int flags, const struct timespec *req,
                    struct timespec *rem);

/* POSIX timezone state, published by tzset(). Owned by docs/archive/0325 Group A
   (docs/archive/0382 gap 6 defers to it). */
extern long timezone;      /* seconds WEST of UTC (opposite sign to tm_gmtoff) */
extern int  daylight;      /* nonzero if the zone observes DST at some point */
extern char *tzname[2];
void tzset(void);
/* clock_settime: the wall clock belongs to the host/browser — a process
   cannot set it. Fails EPERM, like an unprivileged caller on POSIX. */
#include <errno.h>
static inline int clock_settime(clockid_t clk_id, const struct timespec *tp) {
  (void)clk_id; (void)tp; errno = EPERM; return -1;
}
__import int __nanosleep(long sec, long nsec);
static inline int nanosleep(const struct timespec *req, struct timespec *rem) {
  (void)rem;
  return __nanosleep(req->tv_sec, req->tv_nsec);
}
  