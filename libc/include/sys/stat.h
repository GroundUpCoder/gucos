
#pragma once
__require_source("__posix.c");   /* umask + the mkdir wrapper that applies it */
#include <sys/types.h>

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFIFO  0010000
#define S_IFSOCK 0140000
#define S_ISUID  04000
#define S_ISGID  02000
#define S_ISVTX  01000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#include <time.h>   /* struct timespec (one definition; redefining it here
                       would collide with time.h at file scope) */
// 64-bit ABI: st_size, st_blocks and all timestamps are 64-bit so files can
// exceed 4 GiB and times can exceed 2038/2106. Fields are grouped 32-bit-first
// then 8-byte-aligned 64-bit, so there's no internal padding before the wide
// fields. The host's writeStatBuf MUST match this layout byte-for-byte (the
// offsets are pinned by the stdlib/stat_layout test). Under v3 storage these
// fields still carry 32-bit-range values (zero/sign-extended); v4 storage fills
// the full range. tv_nsec is 0 while storage is second-granularity.
struct stat {
  unsigned long st_dev;
  unsigned long st_ino;
  unsigned long st_mode;
  unsigned long st_nlink;
  unsigned long st_rdev;
  unsigned int  st_uid;
  unsigned int  st_gid;
  long          st_blksize;
  long long     st_size;
  long long     st_blocks;
  long long     st_atime;
  long long     st_mtime;
  long long     st_ctime;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
};

/* mkdir()/mkdirat() apply the process umask, so they are real libc functions
   over the raw host import rather than the import itself (docs/archive/0382). The
   host still exports the un-masked mkdir env entry under its own name —
   that name is part of the wasm-ld ABI the clang toolchain links against, so
   the alias is additive and nothing was renamed out from under it. */
__import int __mkdir_impl(const char *path, int mode);
int mkdir(const char *path, mode_t mode);
__import int stat(const char *path, struct stat *buf);
__import int lstat(const char *path, struct stat *buf);
__import int fstat(int fd, struct stat *buf);

/* mknod: no device/special nodes on this fs — always fails. (Regular-file
   creation goes through open(); callers wanting FIFOs get pipes via pipe().)
   mkfifo(3) is deliberately ABSENT rather than stubbed here — a link-testable
   mkfifo that cannot rendezvous is worse than none, because it flips a
   consumer's HAVE_MKFIFO on and moves the failure from configure time to run
   time. Funded by docs/archive/0401, which is also what keeps docs/archive/0382 open. */
static inline int mknod(const char *path, mode_t mode, dev_t dev) {
  (void)path; (void)mode; (void)dev; return -1;
}

/* umask(2) — the process file-mode creation mask (docs/archive/0382 gap 1). Real
   state, really applied: open(O_CREAT), creat(), mkdir() and mkdirat() all
   clear the masked bits before the mode reaches the host, so a program that
   sets a mask observes the modes POSIX says it should.

   The initial mask is the conventional 022. POSIX defines the starting mask
   as INHERITED, and this process model has no fork() to inherit across
   (docs/OS.md's owner-brokered spawn), so the shell default stands in — and
   it is also what keeps every pre-0382 creation mode identical, because the
   filesystem layer used to apply exactly this mask itself. Cross-process
   inheritance through __spawn is a separate, ticketed gap (docs/archive/0399). */
__import int chmod(const char *path, mode_t mode);
__import int fchmod(int fd, mode_t mode);
int fstatat(int dirfd, const char *path, struct stat *buf, int flags);
int mkdirat(int dirfd, const char *path, mode_t mode);
int fchmodat(int dirfd, const char *path, mode_t mode, int flags);

mode_t umask(mode_t mask);
/* Read the mask without disturbing it — POSIX has no such call (you must
   set-and-restore), which is a data race under threads but merely clumsy
   here. The libc's own creation paths use this. */
mode_t __umask_get(void);

#include <sys/time.h>   /* __utime / __futime / gettimeofday */

#define UTIME_NOW  ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

/* utimensat()/futimens(): POSIX 2008 nanosecond-resolution time setting.
   tv_nsec may be UTIME_NOW (use current time) or UTIME_OMIT (leave that one
   unchanged). We honor both — UTIME_OMIT reads the current value via stat()/
   fstat() and writes it back. Sub-second precision is truncated to whole
   seconds (the filesystems here are second-granularity). dirfd is treated as
   AT_FDCWD; AT_SYMLINK_NOFOLLOW is moot (no symlinks). */
static inline int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) {
  time_t a, m;
  struct timeval now; gettimeofday(&now, 0);
  (void)dirfd; (void)flags;
  if (times == 0) { a = m = now.tv_sec; }
  else {
    a = (times[0].tv_nsec == UTIME_NOW) ? now.tv_sec : times[0].tv_sec;
    m = (times[1].tv_nsec == UTIME_NOW) ? now.tv_sec : times[1].tv_sec;
    if (times[0].tv_nsec == UTIME_OMIT || times[1].tv_nsec == UTIME_OMIT) {
      struct stat st;
      if (stat(path, &st) != 0) return -1;
      if (times[0].tv_nsec == UTIME_OMIT) a = st.st_atime;
      if (times[1].tv_nsec == UTIME_OMIT) m = st.st_mtime;
    }
  }
  return __utime(path, a, m);
}
static inline int futimens(int fd, const struct timespec times[2]) {
  time_t a, m;
  struct timeval now; gettimeofday(&now, 0);
  if (times == 0) { a = m = now.tv_sec; }
  else {
    a = (times[0].tv_nsec == UTIME_NOW) ? now.tv_sec : times[0].tv_sec;
    m = (times[1].tv_nsec == UTIME_NOW) ? now.tv_sec : times[1].tv_sec;
    if (times[0].tv_nsec == UTIME_OMIT || times[1].tv_nsec == UTIME_OMIT) {
      struct stat st;
      if (fstat(fd, &st) != 0) return -1;
      if (times[0].tv_nsec == UTIME_OMIT) a = st.st_atime;
      if (times[1].tv_nsec == UTIME_OMIT) m = st.st_mtime;
    }
  }
  return __futime(fd, a, m);
}
  