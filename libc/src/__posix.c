
/* POSIX process/file-creation layer (docs/archive/0382, docs/archive/0325).
 *
 * Home for the calls that need real per-process state or real path work
 * rather than a one-line forward to a host import:
 *   - umask(2) and the creation paths that must consult it
 *   - open(), which lives here (not in __stdio.c) so <fcntl.h> links alone
 *
 * Kept deliberately small: <fcntl.h>, <sys/stat.h> and <unistd.h> all pull
 * this TU in, and the linker does not drop unreferenced TU functions
 * (measured by tools/zipmeasure — see its README), so every byte here is a
 * byte in every binary that opens a file.
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
int setsid(void) { return __spawn_setsid(); }
#include <stdarg.h>
#include <stdio.h>    /* rename() */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>   /* NAME_MAX / PATH_MAX for pathconf */
#include <sys/time.h> /* futimesat */

/* Match the virtual host identity reported by uname(). */
int gethostname(char *name, size_t len) {
  static const char hostname[] = "localhost";
  size_t n = len < sizeof(hostname) ? len : sizeof(hostname);
  if (n) memcpy(name, hostname, n);
  if (len < sizeof(hostname)) { errno = ENAMETOOLONG; return -1; }
  return 0;
}

int fcntl(int fd, int cmd, ...) {
  int arg = 0;
  if (cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW) {
    va_list ap;
    va_start(ap, cmd);
    struct flock *lock = va_arg(ap, struct flock *);
    va_end(ap);
    return __fcntl_lock(fd, cmd, lock);
  }
  if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC || cmd == F_SETFD || cmd == F_SETFL) {
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);
  }
  return __fcntl3(fd, cmd, arg);
}

int gettimeofday(struct timeval *tv, void *tz) {
  (void)tz;
  if (tv) {
    __gettimeofday(&tv->tv_sec, &tv->tv_usec);
  }
  return 0;
}

/* ---- umask ---------------------------------------------------------- */

/* Only the permission bits are maskable (POSIX: the mask is &~'d against the
   mode, and only the file permission bits are defined for it). */
#define __UMASK_BITS 0777

/* Initial mask 022 — the value every real shell hands its children, and the
   one that keeps this libc byte-compatible with the pre-0382 world: the fs
   layer used to apply a hardcoded ~022 of its own (host.js BlockFS.open),
   which has been removed in favour of this. Anything that never calls umask()
   therefore sees exactly the modes it saw before. POSIX says the starting
   mask is copied by fork and carried across exec; fresh spawns default to 022. */
static mode_t __umask_main = 022;
static mode_t *__umask_location(void) {
  unsigned rec = __builtin(thread_record);
  return *(unsigned *)(rec + 2120) ? (mode_t *)(rec + 2124) : &__umask_main;
}
#define __umask_val (*__umask_location())

mode_t umask(mode_t mask) {
  mode_t prev = __umask_val;
  __umask_val = mask & __UMASK_BITS;
  return prev;
}

mode_t __umask_get(void) { return __umask_val; }
unsigned __process_umask(unsigned mask, int set) {
  if (set) __umask_val = mask & 0777;
  return __umask_val;
}

__export __process_umask = __process_umask;

/* exec family: PATH search stays in libc; replacement is owner-brokered. */
static int __exec_search_try(const char *path, char *const argv[]) {
  execve(path,argv,environ);
  if(errno != ENOEXEC) return -1;
  int n=0;while(argv && argv[n])n++;
  char **sh=malloc((n+3)*sizeof(char *));
  if(!sh){errno=ENOMEM;return -1;}
  sh[0]="sh";sh[1]=(char *)path;
  int i=1;for(;i<n;i++)sh[i+1]=argv[i];
  sh[n>0?n+1:2]=0;
  execve("/bin/sh",sh,environ);
  int err=errno;free(sh);errno=err;return -1;
}
int execv(const char *path, char *const argv[]) { return execve(path, argv, environ); }
int execvp(const char *file, char *const argv[]) {
  if (!file || !*file) { errno = ENOENT; return -1; }
  if (strchr(file, '/')) return __exec_search_try(file, argv);
  const char *path = getenv("PATH");
  if (!path) path = "/bin:/usr/bin";
  int denied = 0;
  for (;;) {
    const char *end = strchr(path, ':');
    size_t n = end ? (size_t)(end-path) : strlen(path);
    size_t len = n + strlen(file) + 2;
    char *full = malloc(len);
    if (!full) { errno = ENOMEM; return -1; }
    if (n) { memcpy(full,path,n); full[n]='/'; strcpy(full+n+1,file); }
    else strcpy(full,file);
    __exec_search_try(full, argv);
    int err = errno; free(full);
    if (err == EACCES) denied = 1;
    else if (err != ENOENT && err != ENOTDIR) { errno=err; return -1; }
    if (!end) break;
    path = end+1;
  }
  errno = denied ? EACCES : ENOENT; return -1;
}
static int __exec_list(const char *path, const char *arg, va_list ap, int kind) {
  va_list count; va_copy(count, ap);
  int n = arg ? 1 : 0;
  if (arg) while (va_arg(count, const char *)) n++;
  va_end(count);
  char **argv = malloc((n+1)*sizeof(char *));
  if (!argv) { errno=ENOMEM; return -1; }
  if (n) { argv[0]=(char *)arg; for (int i=1;i<n;i++) argv[i]=va_arg(ap,char *); (void)va_arg(ap,char *); }
  argv[n]=0;
  char **env = kind==1 ? va_arg(ap,char **) : environ;
  int r = kind==2 ? execvp(path,argv) : execve(path,argv,env);
  int err=errno;free(argv);errno=err;return r;
}
int execl(const char *path, const char *arg, ...) { va_list ap;va_start(ap,arg);int r=__exec_list(path,arg,ap,0);va_end(ap);return r; }
int execle(const char *path, const char *arg, ...) { va_list ap;va_start(ap,arg);int r=__exec_list(path,arg,ap,1);va_end(ap);return r; }
int execlp(const char *path, const char *arg, ...) { va_list ap;va_start(ap,arg);int r=__exec_list(path,arg,ap,2);va_end(ap);return r; }

/* ---- creation paths that apply it ------------------------------------ */

/* Variadic wrapper around __open_impl (a non-variadic host import). The mode
   argument is only read — and only masked — when O_CREAT is set, exactly as
   POSIX specifies; without O_CREAT there is no mode argument to read and
   touching the va_list would be undefined behaviour. */
int open(const char *path, int flags, ...) {
  int mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
    mode &= ~(int)__umask_val;
  }
  return __open_impl(path, flags, mode);
}

int creat(const char *path, mode_t mode) {
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, (int)mode);
}

int mkdir(const char *path, mode_t mode) {
  return __mkdir_impl(path, (int)(mode & ~__umask_val));
}

/* ---- getentropy (docs/archive/0325 Group B) ----------------------------------
 *
 * CPython's bootstrap_hash.c uses this to seed hash randomisation, so weak
 * or fabricated entropy here would be a silent security regression rather
 * than a visible gap. Backed by a HOST import over crypto.getRandomValues,
 * NOT by reading /dev/urandom: entropy is a host capability, and routing it
 * through a device node would make a security primitive depend on
 * filesystem layout — which is not hypothetical, since /dev exists only on
 * a v4 mount. If the host has no CSPRNG the call FAILS; it never falls back
 * to something predictable. */
__import int __getentropy(void *buf, int len);

int getentropy(void *buf, size_t buflen) {
  if (buflen > 256) { errno = EIO; return -1; }   /* POSIX/OpenBSD cap */
  if (buflen == 0) return 0;
  if (__getentropy(buf, (int)buflen) != 0) { errno = EIO; return -1; }
  return 0;
}

/* ---- confstr / pathconf / fpathconf (docs/archive/0325 Group B) ---------------
 *
 * Real values, from this system's real limits, so a caller sizing a buffer
 * from pathconf() gets an answer it can trust. An unrecognised name is -1
 * WITHOUT errno set, which POSIX defines as "no limit / not supported" and
 * is distinguishable from a genuine error only by errno — hence the
 * explicit clear. */
size_t confstr(int name, char *buf, size_t len) {
  const char *v;
  if (name == _CS_PATH) v = "/usr/local/bin:/bin";   /* matches the OS PATH */
  else { errno = EINVAL; return 0; }
  size_t n = 0;
  while (v[n]) n++;
  if (buf && len) {
    size_t i = 0;
    for (; i + 1 < len && v[i]; i++) buf[i] = v[i];
    buf[i] = 0;
  }
  return n + 1;                                      /* includes the NUL */
}

static long __pathconf_value(int name) {
  switch (name) {
    case _PC_LINK_MAX:    return 32767;
    case _PC_NAME_MAX:    return NAME_MAX;      /* 255 */
    case _PC_PATH_MAX:    return PATH_MAX;      /* 4096 */
    case _PC_PIPE_BUF:    return 4096;
    case _PC_NO_TRUNC:    return 1;             /* over-long names are an error */
    case _PC_SYMLINK_MAX: return PATH_MAX;
    default:              errno = EINVAL; return -1;
  }
}

long pathconf(const char *path, int name) {
  struct stat st;
  if (stat(path, &st) != 0) return -1;          /* the path must exist */
  errno = 0;
  return __pathconf_value(name);
}

__import int __fchdir(int fd);
int fchdir(int fd) { return __fchdir(fd); }

long fpathconf(int fd, int name) {
  struct stat st;
  if (fstat(fd, &st) != 0) return -1;
  errno = 0;
  return __pathconf_value(name);
}

/* ---- the *at() family (docs/archive/0382 gaps 7-8, docs/archive/0325 Group B) --------
 *
 * OWNED BY docs/archive/0325 Group B, which lists the whole family; docs/archive/0382
 * names openat/fstatat and defers to it. Both tickets record that.
 *
 * POSIX resolution rules, in order:
 *   1. An ABSOLUTE path ignores dirfd entirely.
 *   2. AT_FDCWD resolves against the current working directory.
 *   3. Anything else resolves against the directory the fd refers to.
 *
 * Cases 1 and 2 are implemented. O_DIRECTORY and fchdir now expose real
 * directory descriptors, but the *at family still lacks descriptor-relative
 * path resolution. Such requests return ENOTSUP; applications must select
 * their chdir/fchdir fallback until that separate capability is implemented.
 */
static int __at_ok(int dirfd, const char *path) {
  if (path && path[0] == '/') return 0;      /* absolute: dirfd is ignored */
  if (dirfd == AT_FDCWD) return 0;
  struct stat st;
  if (fstat(dirfd, &st) != 0) { errno = EBADF; return -1; }
  errno = S_ISDIR(st.st_mode) ? ENOTSUP : ENOTDIR;
  return -1;
}

int openat(int dirfd, const char *path, int flags, ...) {
  int mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }
  if (__at_ok(dirfd, path) != 0) return -1;
  return open(path, flags, mode);            /* open() applies the umask */
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
  if (__at_ok(dirfd, path) != 0) return -1;
  return (flags & AT_SYMLINK_NOFOLLOW) ? lstat(path, buf) : stat(path, buf);
}

int mkdirat(int dirfd, const char *path, mode_t mode) {
  if (__at_ok(dirfd, path) != 0) return -1;
  return mkdir(path, mode);
}

int unlinkat(int dirfd, const char *path, int flags) {
  if (__at_ok(dirfd, path) != 0) return -1;
  return (flags & AT_REMOVEDIR) ? rmdir(path) : unlink(path);
}

int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
  if (__at_ok(olddirfd, oldpath) != 0) return -1;
  if (__at_ok(newdirfd, newpath) != 0) return -1;
  return rename(oldpath, newpath);
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
  if (__at_ok(dirfd, path) != 0) return -1;
  /* AT_EACCESS asks for the EFFECTIVE ids. Real and effective ids are both
     root here (see getuid/geteuid in <unistd.h>), so the two agree by
     construction and the flag is a no-op rather than an unimplemented one. */
  (void)flags;
  return access(path, mode);
}

int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
  if (__at_ok(olddirfd, oldpath) != 0) return -1;
  if (__at_ok(newdirfd, newpath) != 0) return -1;
  if (flags & AT_SYMLINK_FOLLOW) {
    /* link(2) never follows a final symlink; AT_SYMLINK_FOLLOW asks it to.
       Resolve first, then link the resolved target. */
    char resolved[4096];
    if (realpath(oldpath, resolved) == 0) return -1;
    return link(resolved, newpath);
  }
  return link(oldpath, newpath);
}

int symlinkat(const char *target, int newdirfd, const char *linkpath) {
  /* NB the target is NOT resolved against newdirfd — it is stored verbatim as
     the link body, which is why only linkpath is checked. */
  if (__at_ok(newdirfd, linkpath) != 0) return -1;
  return symlink(target, linkpath);
}

long readlinkat(int dirfd, const char *path, char *buf, unsigned long bufsize) {
  if (__at_ok(dirfd, path) != 0) return -1;
  return readlink(path, buf, (long)bufsize);
}

int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
  if (__at_ok(dirfd, path) != 0) return -1;
  /* AT_SYMLINK_NOFOLLOW would mean "change the LINK's mode". Link modes are
     not settable on this fs (nor on Linux), so POSIX's ENOTSUP is the
     honest answer rather than silently changing the target instead. */
  if (flags & AT_SYMLINK_NOFOLLOW) { errno = ENOTSUP; return -1; }
  return chmod(path, (int)mode);
}

int fchownat(int dirfd, const char *path, unsigned owner, unsigned group, int flags) {
  if (__at_ok(dirfd, path) != 0) return -1;
  return (flags & AT_SYMLINK_NOFOLLOW) ? lchown(path, owner, group)
                                       : chown(path, owner, group);
}

int futimesat(int dirfd, const char *path, const struct timeval times[2]) {
  if (__at_ok(dirfd, path) != 0) return -1;
  if (times == 0) {
    struct timeval now;
    gettimeofday(&now, 0);
    return __utime(path, now.tv_sec, now.tv_sec);
  }
  /* Sub-second precision is dropped: the filesystems here are
     second-granularity (see utimes() in <sys/time.h>, same treatment). */
  return __utime(path, times[0].tv_sec, times[1].tv_sec);
}

/* ---- truncate / posix_fallocate / posix_fadvise (docs/archive/0325 Group B) ---- */

int truncate(const char *path, off_t length) {
  int fd = open(path, O_WRONLY);
  if (fd < 0) return -1;
  int r = ftruncate(fd, length);
  int saved = errno;          /* close() must not overwrite ftruncate's errno */
  close(fd);
  if (r != 0) errno = saved;
  return r;
}

/* posix_fallocate RETURNS an error number and does NOT set errno — one of
   POSIX's few inversions, and getting it wrong makes every caller's check
   read backwards. Really reserves the space: ftruncate here grows the file's
   extent out of the allocator (host.js _growExtent), so a later write into
   the range cannot fail for want of room, which is the whole guarantee. */
int posix_fallocate(int fd, off_t offset, off_t len) {
  if (offset < 0 || len <= 0) return EINVAL;
  struct stat st;
  if (fstat(fd, &st) != 0) return errno;
  off_t need = offset + len;
  if (need <= st.st_size) return 0;         /* already covered */
  if (ftruncate(fd, need) != 0) return errno;
  return 0;
}
  