
/* Internal runtime ABI must not bind an application pthread shim. */
#include </__wasm_posix__/include/pthread.h>
#include <__thread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
__import int __fork_impl(void);
void __malloc_fork_lock(int acquire);
void __stdio_fork_lock(int acquire);
void __pthread_fork_lock(int acquire);
void __pthread_fork_child(void);
struct __fork_handler {
  void (*prepare)(void), (*parent)(void), (*child)(void);
  struct __fork_handler *next, *prev;
};
static struct __fork_handler *__fork_first, *__fork_last;
static __lock_t __fork_lock = __LOCK_INIT;
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
  struct __fork_handler *h = malloc(sizeof(*h));
  if (!h) return ENOMEM;
  h->prepare = prepare; h->parent = parent; h->child = child; h->next = 0;
  __lock_acquire(&__fork_lock);
  h->prev = __fork_last;
  if (__fork_last) __fork_last->next = h; else __fork_first = h;
  __fork_last = h;
  __lock_release(&__fork_lock);
  return 0;
}
int fork(void) {
  __lock_acquire(&__fork_lock);
  struct __fork_handler *last = __fork_last;
  for (struct __fork_handler *h = last; h; h = h->prev) if (h->prepare) h->prepare();
  /* Acquire libc locks before parking siblings, in their normal lock order. */
  __stdio_fork_lock(1);
  __pthread_fork_lock(1);
  __malloc_fork_lock(1);
  int pid = __fork_impl();
  int saved_errno = errno;
  if (pid == 0) __pthread_fork_child();
  __malloc_fork_lock(0);
  __pthread_fork_lock(0);
  __stdio_fork_lock(0);
  if (last) for (struct __fork_handler *h = __fork_first; h; h = h->next) {
    void (*fn)(void) = pid == 0 ? h->child : h->parent;
    if (fn) fn();
    if (h == last) break;
  }
  __lock_release(&__fork_lock);
  errno = saved_errno;
  return pid;
}
  