
#pragma once
#include <sys/types.h>
#include <unistd.h>   // __spawn_wait (the owner-brokered process model)
#define WNOHANG    0x01
#define WUNTRACED  0x02
#define WCONTINUED 0x08
#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) >> 8) & 0xff)
#define WIFSIGNALED(status)  (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
#define WTERMSIG(status)     ((status) & 0x7f)
#define WIFSTOPPED(status)   (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)     WEXITSTATUS(status)
#define WIFCONTINUED(status) ((status) == 0xffff)
#define WCOREDUMP(status)    ((status) & 0x80)
// Reap a spawned child via the process kernel. status is POSIX-encoded:
// WEXITSTATUS(status) == child's exit code. waitpid(-1, ...) (any child) and
// the pgroup selectors (0 / -pgid) are supported by kernel.js; older external
// owner kernels may fail them with ECHILD.
__require_source("__wait.c");
pid_t waitpid(pid_t pid, int *status, int options);
pid_t wait(int *status);
  