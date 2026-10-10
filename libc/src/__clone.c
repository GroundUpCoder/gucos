
#include <sched.h>
#include <unistd.h>
#include <errno.h>
__import int __vfork_impl(void);
int vfork(void) { return __vfork_impl(); }
__import int __clone_impl(int (*fn)(void *), void *stack, int flags, void *arg);
int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...) {
  if (!fn || !stack) { errno = EINVAL; return -1; }
  if (flags != (CLONE_VM | CLONE_VFORK | SIGCHLD)) { errno = EINVAL; return -1; }
  return __clone_impl(fn, stack, flags, arg);
}
void __clone_entry(int (*fn)(void *), void *arg) { _exit(fn(arg)); }
__export __clone_entry = __clone_entry;
  