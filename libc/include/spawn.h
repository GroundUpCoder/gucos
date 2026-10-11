
#pragma once
#include <sys/types.h>
#include <unistd.h>   // struct __spawn_spec / __fd_action / __spawn
#include <errno.h>
#include <signal.h>   // sigset_t (spawnattr sigdefault/sigmask)
#include <sched.h>
#include <sys/stat.h>
#include <stdlib.h>   // getenv (posix_spawnp PATH search)
#include <string.h>   // strchr/strlen/memcpy/strcpy

// posix_spawn over the owner-brokered process model. file_actions/attr marshal
// 1:1 into __spawn_spec (file_actions ARE the spec's actions); posix_spawn
// returns an errno value (0 = success), not -1. The owner applies the actions
// against the child's inherited fd table before it runs (kernel.js's spawn
// action loop) — DUP2/OPEN/CLOSE/CLOSEFROM, ops 0..3 below, which must stay in
// step with that loop.
typedef struct {
  struct __fd_action *__acts;
  int __n, __cap;
} posix_spawn_file_actions_t;

typedef struct {
  unsigned __flags;
  int __pgrp;
  sigset_t __sigdef;
  sigset_t __sigmask;
  int __policy;
  struct sched_param __param;
} posix_spawnattr_t;

/* Standard flags are declared even when a requested scheduling policy is
   unsupported. Spawn explicitly rejects real-time scheduling; the ordinary
   SCHED_OTHER/priority-zero policy is the browser's only supported policy. */
#define POSIX_SPAWN_AUTOREAP_NP 0x40
#define POSIX_SPAWN_RESETIDS  0x01
#define POSIX_SPAWN_SETPGROUP 0x02
#define POSIX_SPAWN_SETSIGDEF 0x04
#define POSIX_SPAWN_SETSIGMASK 0x08
#define POSIX_SPAWN_SETSCHEDPARAM 0x10
#define POSIX_SPAWN_SETSCHEDULER 0x20

__require_source("__spawn.c");
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int newfd);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *fa, int fd, const char *path, int oflag, unsigned mode);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd);
int posix_spawn_file_actions_addclosefrom_np(posix_spawn_file_actions_t *fa, int from);
int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *fa, const char *path);
int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *fa, const char *path);
int posix_spawnattr_init(posix_spawnattr_t *a);
int posix_spawnattr_destroy(posix_spawnattr_t *a);
int posix_spawnattr_setflags(posix_spawnattr_t *a, short f);
int posix_spawnattr_getflags(const posix_spawnattr_t *a, short *f);
int posix_spawnattr_setpgroup(posix_spawnattr_t *a, int pg);
int posix_spawnattr_getpgroup(const posix_spawnattr_t *a, int *pg);
int posix_spawnattr_setsigdefault(posix_spawnattr_t *a, const sigset_t *s);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *a, sigset_t *s);
int posix_spawnattr_setsigmask(posix_spawnattr_t *a, const sigset_t *s);
int posix_spawnattr_getsigmask(const posix_spawnattr_t *a, sigset_t *s);
int posix_spawnattr_setschedpolicy(posix_spawnattr_t *a, int policy);
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *a, int *policy);
/* Unnamed parameters: gawk's awk.h defines param as a macro. */
int posix_spawnattr_setschedparam(posix_spawnattr_t *, const struct sched_param *);
int posix_spawnattr_getschedparam(const posix_spawnattr_t *, struct sched_param *);
int posix_spawn(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr,
    char *const argv[], char *const envp[]);
int posix_spawnp(pid_t *pid, const char *file,
    const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr,
    char *const argv[], char *const envp[]);

  