
#pragma once
/* time_t, struct timespec and clockid_t. POSIX allows <sys/types.h> to
   define them, and CPython/musl/wasi-libc all rely on it (docs/archive/0325
   Group A) — see __timespec.h for why this is not just #include <time.h>. */
#include <__timespec.h>
typedef long ssize_t;
typedef long long off_t;
typedef unsigned long size_t;
typedef int mode_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
/* id_t (POSIX): an integer type able to hold a pid_t, uid_t or gid_t. All
   three fit in 32 bits here, so one 32-bit type covers the union. Unsigned
   like glibc's — the only signed member (pid_t) is never negative in the
   contexts id_t is used for (waitid/getpriority id arguments). */
typedef unsigned int id_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
/* time_t comes from __timespec.h above */
typedef unsigned long nlink_t;
typedef long long blkcnt_t;
typedef long blksize_t;
  