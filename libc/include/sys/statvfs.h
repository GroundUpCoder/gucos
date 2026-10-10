
#pragma once
#include <sys/types.h>
typedef unsigned long long fsblkcnt_t;
typedef unsigned long long fsfilcnt_t;
struct statvfs {
  unsigned long f_bsize;
  unsigned long f_frsize;
  fsblkcnt_t    f_blocks;
  fsblkcnt_t    f_bfree;
  fsblkcnt_t    f_bavail;
  fsfilcnt_t    f_files;
  fsfilcnt_t    f_ffree;
  fsfilcnt_t    f_favail;
  unsigned long f_fsid;
  unsigned long f_flag;
  unsigned long f_namemax;
};
#define ST_RDONLY 1
#define ST_NOSUID 2
/* Nominal values only — this runtime exposes no real filesystem geometry to C
   (callers like df() see a fixed 4 GiB volume). A BlockFS-backed statvfs that
   reports real free/used blocks is a TODO (needs a host import). */
static inline int statvfs(const char *path, struct statvfs *buf) {
  (void)path;
  if (buf == 0) return -1;
  buf->f_bsize = 4096; buf->f_frsize = 4096;
  buf->f_blocks = 1048576; buf->f_bfree = 1048576; buf->f_bavail = 1048576;
  buf->f_files = 65536; buf->f_ffree = 65536; buf->f_favail = 65536;
  buf->f_fsid = 0; buf->f_flag = 0; buf->f_namemax = 255;
  return 0;
}
static inline int fstatvfs(int fd, struct statvfs *buf) {
  (void)fd;
  return statvfs((const char *)0, buf);
}
  