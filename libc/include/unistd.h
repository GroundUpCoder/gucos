
#pragma once
__require_source("__posix.c");   /* the *at family, truncate, getentropy, *conf */
/* POSIX requires unistd.h to define size_t (and ssize_t/off_t). */
typedef unsigned long size_t;
typedef long ssize_t;
typedef long long off_t;
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
__import int close(int fd);
void closefrom(int lowfd);
__import long read(int fd, void *buf, long count);
__import long write(int fd, const void *buf, long count);
__import long long lseek(int fd, long long offset, int whence);
char *getcwd(char *buf, size_t size);
__require_source("__getcwd.c");
int gethostname(char *name, size_t len);
__import int chdir(const char *path);
int fchdir(int fd);
__import int access(const char *path, int mode);
__import int rmdir(const char *path);
__import int unlink(const char *path);
__import int pipe(int pipefd[2]);
__import int dup(int oldfd);
__import int dup2(int oldfd, int newfd);
__import int getpid(void);
__import int getppid(void);
__import int isatty(int fd);
__require_source("__ttyname.c");
char *ttyname(int fd);
int ttyname_r(int fd, char *buffer, size_t size);
/* There is no terminal login database in the browser runtime. */
#include <errno.h>
static inline char *getlogin(void) { errno = ENOTTY; return (char *)0; }
__import int usleep(unsigned int usec);
/* Kernel mapping granularity; Wasm memory growth uses separate 64 KiB pages. */
static inline int getpagesize(void) { return 4096; }
__import int ftruncate(int fd, long long length);
/* Positioned I/O is one kernel operation: it never changes the shared offset. */
__import long pread(int fd, void *buf, unsigned long count, long long offset);
__import long pwrite(int fd, const void *buf, unsigned long count, long long offset);
__import long readlink(const char *path, char *buf, long bufsize);

/* The *at() family's <unistd.h> half (docs/archive/0325 Group B). See __at_ok() in
   __posix.c for the resolution rules. */
int faccessat(int dirfd, const char *path, int mode, int flags);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
long readlinkat(int dirfd, const char *path, char *buf, unsigned long bufsize);
int unlinkat(int dirfd, const char *path, int flags);
int fchownat(int dirfd, const char *path, unsigned owner, unsigned group, int flags);

/* truncate(2) — the path-based twin of ftruncate. */
int truncate(const char *path, off_t length);

/* getentropy (docs/archive/0325 Group B) — real randomness for CPython's hash
   seeding. Reads /dev/urandom, which host.js backs with
   crypto.getRandomValues; POSIX/OpenBSD cap a single call at 256 bytes. */
int getentropy(void *buf, size_t buflen);

/* confstr/pathconf/fpathconf (docs/archive/0325 Group B). */
#define _CS_PATH 0
#define _PC_LINK_MAX      0
#define _PC_NAME_MAX      3
#define _PC_PATH_MAX      4
#define _PC_PIPE_BUF      5
#define _PC_NO_TRUNC      7
#define _PC_SYMLINK_MAX  19
size_t confstr(int name, char *buf, size_t len);
long pathconf(const char *path, int name);
long fpathconf(int fd, int name);
__import int fsync(int fd);
__import int fdatasync(int fd);
/* sync(): whole-fs flush. Writes reach the backing store as they happen
   (BlockFS has no process-side dirty cache; per-fd durability is fsync),
   so there is nothing extra to flush from here — a no-op by design. */
static inline void sync(void) { }
__import int chmod(const char *path, int mode);
__import int fchmod(int fd, int mode);
__import int link(const char *oldpath, const char *newpath);
/* Single-user ownership: validate the target; changing to nonzero IDs fails. */
int chown(const char *path,unsigned owner,unsigned group);
int lchown(const char *path,unsigned owner,unsigned group);
int fchown(int fd,unsigned owner,unsigned group);
__import unsigned int sleep(unsigned int seconds);
__import int symlink(const char *target, const char *linkpath);
__import int chmod(const char *path, int mode);
__import char *realpath(const char *path, char *resolved);
extern char **environ;

/* ---- Owner-brokered process model ----
   Three low-level host imports the runtime brokers through its process kernel.
   The familiar POSIX surface (posix_spawn family, system, popen, waitpid, kill)
   is C built on top of these (see <spawn.h>/<sys/wait.h>). The host reads the
   spec struct straight from wasm memory at spawn time — fds are declarative
   (file_actions), because a child is a separate instance and cannot inherit fds
   by memory copy. */
struct __fd_action {            /* op: 0=DUP2 1=OPEN 2=CLOSE */
    int op; int fd; int arg; const char *path; int mode;
};
struct __spawn_spec {
    const char *path;           /* program image (a .wasm) */
    char *const *argv;          /* NULL-terminated */
    char *const *envp;          /* NULL => inherit (host walks __get_environ) */
    const char *cwd;            /* NULL => inherit */
    const struct __fd_action *actions;
    int n_actions;
    unsigned flags;             /* __SPAWN_* bits below */
    int pgid;
    int trace;                  /* strace (0046): pipe write-end fd, -1 = none.
                                   Read by the host ONLY under __SPAWN_TRACE
                                   (spec-grows-by-field compatibility). */
    unsigned sigdefault, sigmask, mask; /* read only under the corresponding flags */
};
/* spec.flags bits. */
#define __SPAWN_SETPGID        1u  /* pgid field valid (0 = own pid) */
#define __SPAWN_TRACE          2u  /* trace field valid (docs/archive/0046) */
#define __SPAWN_SIGDEFAULT     8u
#define __SPAWN_SIGMASK       16u
#define __SPAWN_UMASK         32u
#define __SPAWN_AUTOREAP     128u /* detached child, no wait status */
#define __SPAWN_CHECK_EXEC    64u
#define __SPAWN_TRACE_CHILDREN 4u  /* trace descendants too (strace -f) */
__import int __spawn(const struct __spawn_spec *spec);        /* -> pid | -1+errno */
__import int __spawn_wait(int pid, int *status, int options); /* -> pid | -1+errno */
__import int __spawn_kill(int pid, int sig);                  /* -> 0   | -1+errno */
__import int __spawn_setpgid(int pid, int pgid);              /* -> 0   | -1+errno */
__import int __spawn_getpgid(int pid);                        /* -> pgid| -1+errno */
__import int __spawn_getsid(int pid);                         /* -> sid | -1+errno */
__import int __spawn_setsid(void);                            /* -> sid | -1+errno */
/* plain int like the imports above: pid_t's typedef comes later */
static inline int setpgid(int pid, int pgid) { return __spawn_setpgid(pid, pgid); }
static inline int getpgid(int pid)           { return __spawn_getpgid(pid); }
static inline int getpgrp(void)              { return __spawn_getpgid(0); }
static inline int getsid(int pid)            { return __spawn_getsid(pid); }
int setsid(void);
/* Run the host's in-browser C compiler (it has no wasm image to exec). Packs the
   result into buf as: int exitCode, int outLen, int errLen, then outLen stdout
   bytes, then errLen stderr bytes. Returns total bytes written, or -1+errno
   (ENOMEM if buf is too small, ENOSYS without a host kernel). Used by /bin/cc so
   'cc' is a normal command a real shell can spawn. */
__import int __compile(const char *cwd, char *const *argv, char *buf, int cap);

// Process image transitions require the browser process kernel.
#define _SC_OPEN_MAX 4
/* Mapping granularity (docs/MAPPINGS.md), also reported by getpagesize(). */
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 84
__require_source("__fork.c");
int fork(void);
__require_source("__clone.c");
int vfork(void);
__import int execve(const char *path, char *const argv[], char *const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execl(const char *path, const char *arg, ...);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
/* _exit: terminate WITHOUT atexit handlers / stdio flushing (KERNEL.md
   "Exit and teardown": same __exit handshake as exit(), minus step 1).
   Was a spin-forever stub from the pre-kernel era — hush's exit path hung
   on it (docs/archive/0005). The loop after __exit only satisfies noreturn. */
__import void __exit(int status);
static inline void  _exit(int s)            { __exit(s); for (;;) {} }
static inline long  sysconf(int name)       { if (name == _SC_PAGESIZE) return 4096; if (name == _SC_NPROCESSORS_ONLN) return 4; return -1; }
static inline int   setuid(unsigned uid)    { if(!uid)return 0; errno=ENOTSUP; return -1; }
static inline int   setgid(unsigned gid)    { if(!gid)return 0; errno=ENOTSUP; return -1; }
/* kill() lives in __signal.c (declared in signal.h; declared here too for
   unistd-only callers): self-directed signals must deliver through the libc
   handler tables, not just the kernel RPC. __signal.c links into every
   stdlib program via abort(), so the symbol is always present. */
int kill(int __pid, int __sig);
/* alarm()/ualarm() are facades over the kernel's ITIMER_REAL (docs/archive/0044,
   setitimer in <sys/time.h>); like kill(), the implementations live in
   __signal.c. Without a kernel they return 0 with errno ENOSYS. */
unsigned alarm(unsigned __seconds);
unsigned ualarm(unsigned __usecs, unsigned __interval);
/* Single root user: real and effective user/group IDs are all 0 (root).
   These never fail (POSIX getuid/getgid have no error return). */
static inline unsigned getuid(void)         { return 0; }
static inline unsigned geteuid(void)        { return 0; }
static inline unsigned getgid(void)         { return 0; }
static inline unsigned getegid(void)        { return 0; }
  