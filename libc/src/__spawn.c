
#include <spawn.h>
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa) {
  fa->__acts = 0; fa->__n = fa->__cap = 0; return 0;
}
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa) {
  for (int i=0; i<fa->__n; i++) free((void *)fa->__acts[i].path);
  free(fa->__acts); fa->__acts = 0; fa->__n = fa->__cap = 0; return 0;
}
static inline int __spawn_action(posix_spawn_file_actions_t *fa, int op, int fd, int arg, const char *path, int mode) {
  if (fd < 0 || (op == 0 && arg < 0)) return EBADF;
  char *copy = 0;
  if (path) { copy = strdup(path); if (!copy) return ENOMEM; }
  if (fa->__n == fa->__cap) {
    int cap = fa->__cap ? fa->__cap * 2 : 8;
    if (cap < fa->__cap || cap > 1048576) { free(copy); return ENOMEM; }
    struct __fd_action *acts = realloc(fa->__acts, cap * sizeof(*acts));
    if (!acts) { free(copy); return ENOMEM; }
    fa->__acts = acts; fa->__cap = cap;
  }
  struct __fd_action *a = &fa->__acts[fa->__n++];
  a->op=op; a->fd=fd; a->arg=arg; a->path=copy; a->mode=mode;
  return 0;
}
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int newfd) {
  return __spawn_action(fa,0,newfd,fd,0,0);
}
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *fa, int fd, const char *path, int oflag, unsigned mode) {
  if (!path) return EINVAL;
  return __spawn_action(fa,1,fd,oflag,path,mode);
}
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd) {
  return __spawn_action(fa,2,fd,0,0,0);
}
int posix_spawn_file_actions_addclosefrom_np(posix_spawn_file_actions_t *fa, int from) {
  return __spawn_action(fa,3,from,0,0,0);
}
int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *fa, const char *path) {
  if (!path) return EINVAL;
  return __spawn_action(fa,4,0,0,path,0);
}
int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *fa, const char *path) {
  return posix_spawn_file_actions_addchdir_np(fa,path);
}
int posix_spawnattr_init(posix_spawnattr_t *a) {
  a->__flags = 0; a->__pgrp = 0; a->__sigdef = 0; a->__sigmask = 0; a->__policy = SCHED_OTHER; a->__param.sched_priority = 0; return 0;
}
int posix_spawnattr_destroy(posix_spawnattr_t *a) { (void)a; return 0; }
int posix_spawnattr_setflags(posix_spawnattr_t *a, short f) {
  unsigned flags = (unsigned)(unsigned short)f;
  unsigned supported = POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP |
                       POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK |
                       POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_AUTOREAP_NP;
  if (flags & ~supported) return EINVAL;
  a->__flags = flags;
  return 0;
}
int posix_spawnattr_getflags(const posix_spawnattr_t *a, short *f) { if (f) *f = (short)a->__flags; return 0; }
int posix_spawnattr_setpgroup(posix_spawnattr_t *a, int pg) { if (pg < 0) return EINVAL; a->__pgrp = pg; return 0; }
int posix_spawnattr_getpgroup(const posix_spawnattr_t *a, int *pg) { if (pg) *pg = a->__pgrp; return 0; }
int posix_spawnattr_setsigdefault(posix_spawnattr_t *a, const sigset_t *s) {
  a->__sigdef = s ? *s : 0; return 0;
}
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *a, sigset_t *s) {
  if (s) *s = a->__sigdef; return 0;
}
int posix_spawnattr_setsigmask(posix_spawnattr_t *a, const sigset_t *s) {
  a->__sigmask = s ? *s : 0; return 0;
}
int posix_spawnattr_getsigmask(const posix_spawnattr_t *a, sigset_t *s) {
  if (s) *s = a->__sigmask; return 0;
}

int posix_spawnattr_setschedpolicy(posix_spawnattr_t *a, int policy) {
  if (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR) return EINVAL;
  a->__policy = policy; return 0;
}
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *a, int *policy) {
  *policy = a->__policy; return 0;
}
int posix_spawnattr_setschedparam(posix_spawnattr_t *a, const struct sched_param *param) {
  if (param->sched_priority < 0) return EINVAL;
  a->__param = *param; return 0;
}
int posix_spawnattr_getschedparam(const posix_spawnattr_t *a, struct sched_param *param) {
  *param = a->__param; return 0;
}

int posix_spawn(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr,
    char *const argv[], char *const envp[]) {
  if (attr && (attr->__flags & (POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER))) {
    if (attr->__param.sched_priority != 0 ||
        ((attr->__flags & POSIX_SPAWN_SETSCHEDULER) && attr->__policy != SCHED_OTHER)) return ENOTSUP;
  }
  struct __spawn_spec spec;
  spec.path = path;
  spec.argv = argv;
  spec.envp = envp;          // NULL => inherit
  spec.cwd = 0;              // inherit the parent's cwd
  spec.actions = fa ? fa->__acts : (const struct __fd_action *)0;
  spec.n_actions = fa ? fa->__n : 0;
  spec.flags = (attr && (attr->__flags & POSIX_SPAWN_SETPGROUP)) ? __SPAWN_SETPGID : 0u;
  spec.pgid = attr ? attr->__pgrp : 0;
  spec.trace = -1;
  spec.flags |= __SPAWN_UMASK | __SPAWN_CHECK_EXEC;
  if (attr && (attr->__flags & POSIX_SPAWN_AUTOREAP_NP)) spec.flags |= __SPAWN_AUTOREAP;
  spec.mask = __umask_get();
  spec.sigdefault = attr ? attr->__sigdef : 0;
  spec.sigmask = attr ? attr->__sigmask : 0;
  if (attr && (attr->__flags & POSIX_SPAWN_SETSIGDEF)) spec.flags |= __SPAWN_SIGDEFAULT;
  if (attr && (attr->__flags & POSIX_SPAWN_SETSIGMASK)) spec.flags |= __SPAWN_SIGMASK;
  int r = __spawn(&spec);
  if (r < 0) return errno ? errno : ENOSYS;   // posix_spawn returns errno, not -1
  if (pid) *pid = r;
  return 0;
}
/* Search every PATH entry, retaining EACCES when no candidate succeeds. */
int posix_spawnp(pid_t *pid, const char *file,
    const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr,
    char *const argv[], char *const envp[]) {
  if (!file || !*file) return ENOENT;
  if (strchr(file, '/')) return posix_spawn(pid,file,fa,attr,argv,envp);
  const char *path = getenv("PATH");
  if (!path) path = "/bin:/usr/bin";
  size_t flen = strlen(file);
  int denied = 0;
  for (;;) {
    const char *colon = strchr(path, ':');
    size_t dlen = colon ? (size_t)(colon-path) : strlen(path);
    char *candidate = malloc(dlen+flen+3);
    if (!candidate) return ENOMEM;
    size_t k=0;
    if (dlen) { memcpy(candidate,path,dlen); k=dlen; } else candidate[k++]='.';
    candidate[k++]='/';strcpy(candidate+k,file);
    int r=posix_spawn(pid,candidate,fa,attr,argv,envp);
    free(candidate);
    if (!r) return 0;
    if (r==EACCES) denied=1;
    else if (r!=ENOENT && r!=ENOTDIR) return r;
    if (!colon) break;
    path=colon+1;
  }
  return denied ? EACCES : ENOENT;
}

