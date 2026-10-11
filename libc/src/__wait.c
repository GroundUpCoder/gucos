
#include <sys/wait.h>
pid_t waitpid(pid_t pid, int *status, int options) {
  return __spawn_wait(pid, status, options);
}
pid_t wait(int *status) { return waitpid(-1, status, 0); }
