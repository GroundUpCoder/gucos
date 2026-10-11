
#pragma once
__require_source("__posix.c");   /* open() — <fcntl.h> declares it, so <fcntl.h> must link it */
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>   /* posix_fadvise validates its fd with fstat */
#include <errno.h>
#define O_ACCMODE 3
#define O_DIRECTORY 0x10000
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x40
#define O_EXCL    0x80
#define O_NOCTTY  0x100
#define O_TRUNC   0x200
#define O_APPEND  0x400
#define O_NONBLOCK 0x800
#define O_NOFOLLOW 0x20000
#define O_CLOEXEC  0x80000

#define F_DUPFD    0
#define F_GETFD    1
#define F_SETFD    2
#define F_GETFL    3
#define F_SETFL    4
#define F_GETLK    5
#define F_SETLK    6
#define F_SETLKW   7
#define FD_CLOEXEC 1

#define F_RDLCK  0
#define F_WRLCK  1
#define F_UNLCK  2

/* *at() anchors and flags (Linux values, like the rest of this runtime).
   Resolution rules and the reason a real dirfd cannot occur here are
   documented at __at_ok() in __posix.c (docs/archive/0325 Group B, docs/archive/0400). */
#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200

struct flock {
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};

__import int __open_impl(const char *path, int flags, int mode);
int open(const char *path, int flags, ...);
/* creat(path, mode) == open(path, O_WRONLY|O_CREAT|O_TRUNC, mode); it is a
   creation call, so it is umask-masked like the rest (docs/archive/0382). */
int creat(const char *path, mode_t mode);
#define F_DUPFD_CLOEXEC 1030  /* Linux value; tracked by the process kernel */
/* Real fcntl for the int-argument commands (F_DUPFD and friends reach the
 * host — the shell's fd-save dance needs them, docs/archive/0005). The lock
 * commands (F_GETLK, F_SETLK, F_SETLKW) are POSIX record locks kept by the
 * process kernel (docs/MAPPINGS.md). */
__import int __fcntl3(int fd, int cmd, int arg);
__import int __fcntl_lock(int fd, int cmd, struct flock *lock);

/* openat + the fd-based allocation calls (docs/archive/0325 Group B). */
int openat(int dirfd, const char *path, int flags, ...);

/* posix_fadvise: the access-pattern hint. A no-op IS the correct
   implementation — POSIX defines it as advisory and explicitly permits an
   implementation to ignore it — but it still validates its arguments, so a
   bad fd or a bogus advice value is reported rather than silently accepted.
   Like posix_fallocate it RETURNS an error number and does not set errno. */
#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5
int posix_fallocate(int fd, off_t offset, off_t len);
static inline int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
  struct stat __st;
  (void)offset; (void)len;
  if (advice < POSIX_FADV_NORMAL || advice > POSIX_FADV_NOREUSE) return EINVAL;
  if (fstat(fd, &__st) != 0) return EBADF;
  return 0;
}
int fcntl(int fd, int cmd, ...);
  