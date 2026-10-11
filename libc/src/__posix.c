
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

/* Descriptor-relative operations are resolved atomically by the filesystem
 * broker. No temporary chdir and no path cached across kernel requests. */
__import int __at_op(int op, int fd, const char *path, int fd2, const char *path2,
                    long long a, long long b, void *out);
static int __at_flags(int flags, int allowed) {
  if (flags & ~allowed) { errno=EINVAL; return -1; } return 0;
}
int openat(int fd,const char *p,int flags,...) {
  int mode=0; if(flags&O_CREAT){va_list ap;va_start(ap,flags);mode=va_arg(ap,int);va_end(ap);}
  return __at_op(0,fd,p,0,0,flags,mode & ~__umask_val,0);
}
int fstatat(int fd,const char *p,struct stat *st,int flags) {
  if(__at_flags(flags,AT_SYMLINK_NOFOLLOW))return -1;
  return __at_op(flags&AT_SYMLINK_NOFOLLOW?2:1,fd,p,0,0,0,0,st);
}
int mkdirat(int fd,const char *p,mode_t mode){return __at_op(3,fd,p,0,0,mode & ~__umask_val,0,0);}
int unlinkat(int fd,const char *p,int flags){if(__at_flags(flags,AT_REMOVEDIR))return -1;return __at_op(flags&AT_REMOVEDIR?5:4,fd,p,0,0,0,0,0);}
int renameat(int fd,const char *p,int fd2,const char *p2){return __at_op(6,fd,p,fd2,p2,0,0,0);}
int faccessat(int fd,const char *p,int mode,int flags){if(__at_flags(flags,AT_EACCESS))return -1;return __at_op(7,fd,p,0,0,mode,0,0);}
int linkat(int fd,const char *p,int fd2,const char *p2,int flags){if(__at_flags(flags,AT_SYMLINK_FOLLOW))return -1;return __at_op(8,fd,p,fd2,p2,flags,0,0);}
int symlinkat(const char *target,int fd,const char *p){return __at_op(9,fd,p,0,target,0,0,0);}
long readlinkat(int fd,const char *p,char *buf,unsigned long size){return __at_op(10,fd,p,0,0,size,0,buf);}
int fchmodat(int fd,const char *p,mode_t mode,int flags){
  if(__at_flags(flags,AT_SYMLINK_NOFOLLOW))return -1;
  if(flags&AT_SYMLINK_NOFOLLOW){errno=ENOTSUP;return -1;}
  return __at_op(11,fd,p,0,0,mode,0,0);
}
int fchownat(int fd,const char *p,unsigned owner,unsigned group,int flags){
  struct stat st;if(fstatat(fd,p,&st,flags))return -1;
  if((owner!=0&&owner!=(unsigned)-1)||(group!=0&&group!=(unsigned)-1)){errno=ENOTSUP;return -1;}return 0;
}
int futimesat(int fd,const char *p,const struct timeval times[2]){
  struct timeval now;if(!times){gettimeofday(&now,0);return __at_op(12,fd,p,0,0,now.tv_sec,now.tv_sec,0);}
  return __at_op(12,fd,p,0,0,times[0].tv_sec,times[1].tv_sec,0);
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

int chown(const char *p,unsigned u,unsigned g){return fchownat(AT_FDCWD,p,u,g,0);}
int lchown(const char *p,unsigned u,unsigned g){return fchownat(AT_FDCWD,p,u,g,AT_SYMLINK_NOFOLLOW);}
int fchown(int fd,unsigned u,unsigned g){struct stat st;if(fstat(fd,&st))return -1;if((u&&u!=(unsigned)-1)||(g&&g!=(unsigned)-1)){errno=ENOTSUP;return -1;}return 0;}
