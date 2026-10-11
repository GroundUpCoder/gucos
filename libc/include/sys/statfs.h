
#pragma once
#include <sys/types.h>
typedef struct { int val[2]; } __fsid_t;
struct statfs {
  long               f_type;
  long               f_bsize;
  unsigned long long f_blocks;
  unsigned long long f_bfree;
  unsigned long long f_bavail;
  unsigned long long f_files;
  unsigned long long f_ffree;
  __fsid_t           f_fsid;
  long               f_namelen;
  long               f_frsize;
  long               f_flags;
  long               f_spare[4];
};
/* Nominal values only (see <sys/statvfs.h>): a fixed 4 GiB volume; real
   BlockFS geometry would need a host import. */
static inline int statfs(const char *path, struct statfs *buf) {
  (void)path;
  if (buf == 0) return -1;
  buf->f_type = 0; buf->f_bsize = 4096; buf->f_frsize = 4096;
  buf->f_blocks = 1048576; buf->f_bfree = 1048576; buf->f_bavail = 1048576;
  buf->f_files = 65536; buf->f_ffree = 65536;
  buf->f_fsid.val[0] = 0; buf->f_fsid.val[1] = 0;
  buf->f_namelen = 255; buf->f_flags = 0;
  buf->f_spare[0] = buf->f_spare[1] = buf->f_spare[2] = buf->f_spare[3] = 0;
  return 0;
}
static inline int fstatfs(int fd, struct statfs *buf) {
  (void)fd;
  return statfs((const char *)0, buf);
}
  