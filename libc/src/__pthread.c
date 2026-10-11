
/* Internal runtime ABI must not bind an application pthread shim. */
#include </__wasm_posix__/include/pthread.h>
#include <__thread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

/* The runtime creates a worker that instantiates this module on the shared
   memory with the record and stack pointer preset, then calls __thread_main. */
__import int __thread_spawn(unsigned record, unsigned size);
#ifdef __WASM_POSIX_SHARED_LIBC
__import unsigned __dl_main_tls_size(void);
__import unsigned __dl_main_tls_align(void);
#endif

#define REC(rec, off) __THREAD_FIELD(rec, off)
#define SELF() ((unsigned)__builtin(thread_record))
#define DEFAULT_STACK (2u << 20)
#define MIN_STACK 81920u

static void __threads_lock(void) {
  while (__builtin(atomic_cmpxchg32, (int *)__THREAD_LOCKED, 0, 1) != 0)
    __thread_wait((int *)__THREAD_LOCKED, 1, -1LL);
}
static void __threads_unlock(void) {
  __builtin(atomic_store32, (int *)__THREAD_LOCKED, 0);
  __builtin(atomic_notify, (int *)__THREAD_LOCKED, 1);
}
static unsigned __process_main = __THREAD_MAIN;
static unsigned __all_threads;   /* head of the record list, guarded above */

static struct __pthread_cleanup *__cleanup_head;
void __pthread_cleanup_push(struct __pthread_cleanup *p,void (*fn)(void *),void *arg){
  p->routine=fn;p->arg=arg;p->owner=SELF();__threads_lock();p->next=__cleanup_head;__cleanup_head=p;__threads_unlock();
}
void __pthread_cleanup_pop(struct __pthread_cleanup *p,int execute){
  __threads_lock();struct __pthread_cleanup **q=&__cleanup_head;while(*q&&*q!=p)q=&(*q)->next;
  int found=*q!=0;if(found)*q=p->next;__threads_unlock();if(found&&execute)p->routine(p->arg);
}
static void __run_cleanups(void){
  for(;;){__threads_lock();struct __pthread_cleanup **q=&__cleanup_head;
    while(*q&&(*q)->owner!=SELF())q=&(*q)->next;
    struct __pthread_cleanup *p=*q;if(p)*q=p->next;__threads_unlock();if(!p)break;p->routine(p->arg);
  }
}

static void __thread_release(unsigned rec) {
  void *raw = (void *)REC(rec, __THREAD_RAW);
  REC(rec, __THREAD_RAW) = 0;
  free(raw);
}

/* Free exited detached threads; runs under the list lock. */
static void __thread_reap(void) {
  unsigned *link = &__all_threads;
  while (*link) {
    unsigned rec = *link;
    if (REC(rec, __THREAD_DETACHED) && __builtin(atomic_load32, (int *)(rec + __THREAD_STATE))) {
      *link = REC(rec, __THREAD_NEXT);
      __thread_release(rec);
    } else link = (unsigned *)(rec + __THREAD_NEXT);
  }
}

static void __thread_unlink(unsigned rec) {
  for (unsigned *link = &__all_threads; *link; link = (unsigned *)(*link + __THREAD_NEXT))
    if (*link == rec) { *link = REC(rec, __THREAD_NEXT); break; }
}

int pthread_create(pthread_t *out, const pthread_attr_t *attr, void *(*start)(void *), void *arg) {
  unsigned size = attr && attr->stacksize ? (unsigned)attr->stacksize : DEFAULT_STACK;
  if (size < MIN_STACK) size = MIN_STACK;
  if (size > 0x7fff0000U) return EAGAIN;
  size = (size + 65535u) & ~65535u;
#ifdef __WASM_POSIX_SHARED_LIBC
  unsigned tls_size = __dl_main_tls_size(), tls_align = __dl_main_tls_align();
#else
  unsigned tls_size = __builtin(tls_size), tls_align = __builtin(tls_align);
#endif
  if (size > 0x7fffffffU - tls_size - tls_align - 16) return EAGAIN;
  char *raw = malloc(size + 16 + tls_size + tls_align);
  if (!raw) return EAGAIN;
  unsigned rec = ((unsigned)raw + 15u) & ~15u;
  memset((void *)rec, 0, __THREAD_RECORD_BYTES);
  REC(rec, __THREAD_LIMIT) = rec + size;
  REC(rec, __THREAD_ARENA) = size;
  REC(rec, __THREAD_SIGBLOCK) = REC(SELF(), __THREAD_SIGBLOCK);
  REC(rec, __THREAD_FN) = (unsigned)start;
  REC(rec, __THREAD_ARG) = (unsigned)arg;
  REC(rec, __THREAD_RAW) = (unsigned)raw;
  REC(rec, __THREAD_TLS) = (rec + size + tls_align - 1) & ~(tls_align - 1);
  REC(rec, __THREAD_DETACHED) = attr ? (unsigned)attr->detached : 0u;
  __threads_lock();
  __thread_reap();
  REC(rec, __THREAD_NEXT) = __all_threads;
  __all_threads = rec;
  __threads_unlock();
  __builtin(atomic_add32, (int *)__THREAD_LIVE, 1);
  int tid = __thread_spawn(rec, size);
  if (tid < 0) {
    __builtin(atomic_sub32, (int *)__THREAD_LIVE, 1);
    __threads_lock();
    __thread_unlink(rec);
    __threads_unlock();
    free(raw);
    return EAGAIN;
  }
  REC(rec, __THREAD_TID) = (unsigned)tid;
  if (out) *out = rec;
  return 0;
}

static void __thread_run_destructors(unsigned rec);

/* Entry point called by the thread worker once its instance is bound to the record. */
void __thread_main(void) {
  unsigned rec = SELF();
  void *(*start)(void *) = (void *(*)(void *))REC(rec, __THREAD_FN);
  sigset_t inherited_mask = REC(rec, __THREAD_SIGBLOCK);
  sigprocmask(SIG_SETMASK, &inherited_mask, 0);
  void *ret;
  if (setjmp((int *)(rec + __THREAD_EXITBUF)) == 0) ret = start((void *)REC(rec, __THREAD_ARG));
  else ret = (void *)REC(rec, __THREAD_RETVAL);
  REC(rec, __THREAD_RETVAL) = (unsigned)ret;
  __thread_run_destructors(rec);
  /* The worker publishes the exited state after every frame has unwound. */
}
__export __thread_main = __thread_main;

void pthread_exit(void *ret) {
  __run_cleanups();
  unsigned rec = SELF();
  REC(rec, __THREAD_RETVAL) = (unsigned)ret;
  if (rec == __process_main) {
    /* The main thread keeps the process alive until the others finish. */
    for (;;) {
      int live = __builtin(atomic_load32, (int *)__THREAD_LIVE);
      if (live <= 0) break;
      __thread_wait((int *)__THREAD_LIVE, live, -1LL);
    }
    exit(0);
  }
  longjmp((int *)(rec + __THREAD_EXITBUF), 1);
}

int pthread_join(pthread_t t, void **retval) {
  unsigned rec = (unsigned)t;
  if (rec == SELF()) return EDEADLK;
  for (;;) {
    int state = __builtin(atomic_load32, (int *)(rec + __THREAD_STATE));
    if (state) break;
    __thread_wait((int *)(rec + __THREAD_STATE), 0, -1LL);
  }
  if (retval) *retval = (void *)REC(rec, __THREAD_RETVAL);
  __threads_lock();
  __thread_unlink(rec);
  __threads_unlock();
  __thread_release(rec);
  return 0;
}

int pthread_detach(pthread_t t) {
  unsigned rec = (unsigned)t;
  if (rec == __process_main) return EINVAL;
  REC(rec, __THREAD_DETACHED) = 1;
  __threads_lock();
  __thread_reap();
  __threads_unlock();
  return 0;
}

pthread_t pthread_self(void) { return SELF(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
int pthread_yield(void) { return sched_yield(); }
int pthread_setname_np(pthread_t t, const char *name) { (void)t; (void)name; return ENOTSUP; }
int pthread_getname_np(pthread_t t, char *name, size_t len) { (void)t; (void)name; (void)len; return ENOTSUP; }
int pthread_kill(pthread_t t, int sig) { if (sig == 0) return 0; if (t == __process_main) { raise(sig); return 0; } return ENOTSUP; }
int pthread_cancel(pthread_t t) { (void)t; return ENOTSUP; }
int pthread_setcancelstate(int state, int *old) { (void)old; return (state==PTHREAD_CANCEL_ENABLE||state==PTHREAD_CANCEL_DISABLE)?ENOTSUP:EINVAL; }
int pthread_setcanceltype(int type, int *old) { (void)old; return (type==PTHREAD_CANCEL_DEFERRED||type==PTHREAD_CANCEL_ASYNCHRONOUS)?ENOTSUP:EINVAL; }
void pthread_testcancel(void) {}


int pthread_attr_init(pthread_attr_t *a) { a->stacksize = 0; a->detached = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t size) { if (size < PTHREAD_STACK_MIN) return EINVAL; a->stacksize = (unsigned)size; return 0; }
int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *size) { *size = a->stacksize ? a->stacksize : DEFAULT_STACK; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int state) { if(state!=PTHREAD_CREATE_JOINABLE&&state!=PTHREAD_CREATE_DETACHED)return EINVAL;a->detached = state; return 0; }
int pthread_attr_getdetachstate(const pthread_attr_t *a, int *state) { *state = a->detached; return 0; }

/* ---- mutex: futex protocol (0 free, 1 held, 2 held with waiters) ---- */
static int __mutex_lock_timed(pthread_mutex_t *m, long long timeout_ns) {
  unsigned self = SELF();
  if (m->lock && m->owner == self) {
    if (m->type == PTHREAD_MUTEX_RECURSIVE) { m->count++; return 0; }
    if (m->type == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
  }
  int c = __builtin(atomic_cmpxchg32, &m->lock, 0, 1);
  if (c) {
    if (c != 2) c = __builtin(atomic_xchg32, &m->lock, 2);
    while (c) {
      if (__thread_wait(&m->lock, 2, timeout_ns) == 2) return ETIMEDOUT;
      c = __builtin(atomic_xchg32, &m->lock, 2);
    }
  }
  m->owner = self; m->count = 1;
  return 0;
}
int pthread_mutex_lock(pthread_mutex_t *m) { return __mutex_lock_timed(m, -1LL); }
int pthread_mutex_trylock(pthread_mutex_t *m) {
  unsigned self = SELF();
  if (m->lock && m->owner == self && m->type == PTHREAD_MUTEX_RECURSIVE) { m->count++; return 0; }
  if (__builtin(atomic_cmpxchg32, &m->lock, 0, 1) != 0) return EBUSY;
  m->owner = self; m->count = 1;
  return 0;
}
static long long __abstime_to_timeout(const struct timespec *abstime, clockid_t clock) {
  struct timespec now;
  clock_gettime(clock, &now);
  long long ns = (long long)(abstime->tv_sec - now.tv_sec) * 1000000000LL + (abstime->tv_nsec - now.tv_nsec);
  return ns < 0 ? 0 : ns;
}
int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abstime) { return __mutex_lock_timed(m, __abstime_to_timeout(abstime, CLOCK_REALTIME)); }
int pthread_mutex_unlock(pthread_mutex_t *m) {
  if (m->type == PTHREAD_MUTEX_ERRORCHECK && (!m->lock || m->owner != SELF())) return EPERM;
  if (m->type == PTHREAD_MUTEX_RECURSIVE && --m->count > 0) return 0;
  m->owner = 0; m->count = 0;
  if (__builtin(atomic_sub32, &m->lock, 1) != 1) {
    __builtin(atomic_store32, &m->lock, 0);
    __builtin(atomic_notify, &m->lock, 1);
  }
  return 0;
}
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { m->lock = 0; m->owner = 0; m->count = 0; m->type = a ? a->type : PTHREAD_MUTEX_NORMAL; return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { return m->lock ? EBUSY : 0; }
int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_NORMAL; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { if (type < 0 || type > 2) return EINVAL; a->type = type; return 0; }
int pthread_mutexattr_gettype(const pthread_mutexattr_t *a, int *type) { *type = a->type; return 0; }
int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int pshared) { (void)a; return pshared==PTHREAD_PROCESS_PRIVATE?0:pshared==PTHREAD_PROCESS_SHARED?ENOTSUP:EINVAL; }

/* ---- condition variables: sequence word plus futex wait ---- */
static int __cond_wait_timed(pthread_cond_t *c, pthread_mutex_t *m, long long timeout_ns) {
  int seq = __builtin(atomic_load32, &c->seq);
  __builtin(atomic_add32, &c->waiters, 1);
  pthread_mutex_unlock(m);
  int r = __thread_wait(&c->seq, seq, timeout_ns);
  __builtin(atomic_sub32, &c->waiters, 1);
  pthread_mutex_lock(m);
  return r == 2 ? ETIMEDOUT : 0;
}
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { return __cond_wait_timed(c, m, -1LL); }
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime) { if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) return EINVAL; return __cond_wait_timed(c, m, __abstime_to_timeout(abstime, c->clock)); }
int pthread_cond_signal(pthread_cond_t *c) {
  if (__builtin(atomic_load32, &c->waiters)) { __builtin(atomic_add32, &c->seq, 1); __builtin(atomic_notify, &c->seq, 1); }
  return 0;
}
int pthread_cond_broadcast(pthread_cond_t *c) {
  if (__builtin(atomic_load32, &c->waiters)) { __builtin(atomic_add32, &c->seq, 1); __builtin(atomic_notify, &c->seq, 0x7fffffff); }
  return 0;
}
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { c->seq = 0; c->waiters = 0; c->clock = a ? a->clock : CLOCK_REALTIME; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_condattr_init(pthread_condattr_t *a) { a->clock = CLOCK_REALTIME; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clock) { if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) return EINVAL; a->clock = clock; return 0; }
int pthread_condattr_getclock(const pthread_condattr_t *a, clockid_t *clock) { *clock = a->clock; return 0; }

/* ---- once ---- */
int pthread_once(pthread_once_t *once, void (*init)(void)) {
  if (__builtin(atomic_load32, once) == 2) return 0;
  if (__builtin(atomic_cmpxchg32, once, 0, 1) == 0) {
    init();
    __builtin(atomic_store32, once, 2);
    __builtin(atomic_notify, once, 0x7fffffff);
    return 0;
  }
  while (__builtin(atomic_load32, once) != 2) __thread_wait(once, 1, -1LL);
  return 0;
}

/* ---- thread-specific data ---- */
static void (*__key_dtor[PTHREAD_KEYS_MAX])(void *);
static unsigned char __key_used[PTHREAD_KEYS_MAX];
static __lock_t __key_lock = __LOCK_INIT;
void __pthread_fork_lock(int acquire) {
  if (acquire) { __threads_lock(); __lock_acquire(&__key_lock); }
  else { __lock_release(&__key_lock); __threads_unlock(); }
}
void __pthread_fork_child(void) {
  /* Only the caller survives. Its stack, TLS and pthread identity stay put.
     Other arenas remain allocated: freeing a possibly active stack is unsafe. */
  __all_threads = 0;
  __process_main = SELF();
  __builtin(atomic_store32, (int *)__THREAD_LIVE, 0);
  REC(SELF(), __THREAD_NEXT) = 0;
  REC(SELF(), __THREAD_SIGPENDING) = 0;
}

int pthread_key_create(pthread_key_t *key, void (*dtor)(void *)) {
  __lock_acquire(&__key_lock);
  for (unsigned i = 0; i < PTHREAD_KEYS_MAX; i++) {
    if (!__key_used[i]) { __key_used[i] = 1; __key_dtor[i] = dtor; *key = i; __lock_release(&__key_lock); return 0; }
  }
  __lock_release(&__key_lock);
  return EAGAIN;
}
int pthread_key_delete(pthread_key_t key) {
  if (key >= PTHREAD_KEYS_MAX) return EINVAL;
  __lock_acquire(&__key_lock); __key_used[key] = 0; __key_dtor[key] = 0; __lock_release(&__key_lock);
  return 0;
}
static void **__tsd(unsigned rec, int create) {
  void **tsd = (void **)REC(rec, __THREAD_TSD);
  if (!tsd && create) { tsd = calloc(PTHREAD_KEYS_MAX, sizeof(void *)); REC(rec, __THREAD_TSD) = (unsigned)tsd; }
  return tsd;
}
void *pthread_getspecific(pthread_key_t key) {
  if (key >= PTHREAD_KEYS_MAX) return 0;
  void **tsd = __tsd(SELF(), 0);
  return tsd ? tsd[key] : 0;
}
int pthread_setspecific(pthread_key_t key, const void *value) {
  if (key >= PTHREAD_KEYS_MAX || !__key_used[key]) return EINVAL;
  void **tsd = __tsd(SELF(), 1);
  if (!tsd) return ENOMEM;
  tsd[key] = (void *)value;
  return 0;
}
static void __thread_run_destructors(unsigned rec) {
  void **tsd = __tsd(rec, 0);
  if (!tsd) return;
  for (int round = 0; round < PTHREAD_DESTRUCTOR_ITERATIONS; round++) {
    int again = 0;
    for (unsigned i = 0; i < PTHREAD_KEYS_MAX; i++) {
      void *v = tsd[i];
      if (v && __key_used[i] && __key_dtor[i]) { tsd[i] = 0; __key_dtor[i](v); again = 1; }
    }
    if (!again) break;
  }
  free(tsd);
  REC(rec, __THREAD_TSD) = 0;
}

/* ---- rwlock: 0 free, positive reader count, -1 exclusive writer ----
   Reader preference permits recursive read acquisition, including while a
   writer waits. Atomics protect the state; wait/notify parks blocked threads.
   Keep the previous 16-byte ABI so existing shared libraries remain usable. */
int pthread_rwlock_init(pthread_rwlock_t *rw, const pthread_rwlockattr_t *a) {
  (void)a; rw->state = 0;
  rw->reserved1 = rw->reserved2 = rw->reserved3 = 0;
  return 0;
}
int pthread_rwlock_destroy(pthread_rwlock_t *rw) {
  return __builtin(atomic_load32, &rw->state) ? EBUSY : 0;
}
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw) {
  for (;;) {
    int state = __builtin(atomic_load32, &rw->state);
    if (state < 0) return EBUSY;
    if (state == 0x7fffffff) return EAGAIN;
    if (__builtin(atomic_cmpxchg32, &rw->state, state, state + 1) == state) return 0;
  }
}
int pthread_rwlock_rdlock(pthread_rwlock_t *rw) {
  for (;;) {
    int result = pthread_rwlock_tryrdlock(rw);
    if (result != EBUSY) return result;
    __thread_wait(&rw->state, -1, -1LL);
  }
}
int pthread_rwlock_trywrlock(pthread_rwlock_t *rw) {
  return __builtin(atomic_cmpxchg32, &rw->state, 0, -1) == 0 ? 0 : EBUSY;
}
int pthread_rwlock_wrlock(pthread_rwlock_t *rw) {
  for (;;) {
    int state = __builtin(atomic_cmpxchg32, &rw->state, 0, -1);
    if (state == 0) return 0;
    __thread_wait(&rw->state, state, -1LL);
  }
}
int pthread_rwlock_unlock(pthread_rwlock_t *rw) {
  for (;;) {
    int state = __builtin(atomic_load32, &rw->state);
    if (state == 0) return EPERM;
    int next = state == -1 ? 0 : state - 1;
    if (__builtin(atomic_cmpxchg32, &rw->state, state, next) != state) continue;
    if (next == 0) __builtin(atomic_notify, &rw->state, 0x7fffffff);
    return 0;
  }
}

int pthread_barrier_init(pthread_barrier_t *b, const pthread_barrierattr_t *a, unsigned count) {
  (void)a; if (!count) return EINVAL;
  pthread_mutex_init(&b->m, 0); pthread_cond_init(&b->c, 0); b->total = (int)count; b->count = 0; b->gen = 0; return 0;
}
int pthread_barrier_destroy(pthread_barrier_t *b) { (void)b; return 0; }
int pthread_barrier_wait(pthread_barrier_t *b) {
  pthread_mutex_lock(&b->m);
  int gen = b->gen;
  if (++b->count == b->total) { b->count = 0; b->gen++; pthread_cond_broadcast(&b->c); pthread_mutex_unlock(&b->m); return PTHREAD_BARRIER_SERIAL_THREAD; }
  while (gen == b->gen) pthread_cond_wait(&b->c, &b->m);
  pthread_mutex_unlock(&b->m);
  return 0;
}
int pthread_spin_init(pthread_spinlock_t *l, int pshared) { if(pshared!=PTHREAD_PROCESS_PRIVATE)return pshared==PTHREAD_PROCESS_SHARED?ENOTSUP:EINVAL;*l = 0; return 0; }
int pthread_spin_destroy(pthread_spinlock_t *l) { (void)l; return 0; }
int pthread_spin_lock(pthread_spinlock_t *l) { while (__builtin(atomic_cmpxchg32, l, 0, 1) != 0) __thread_wait(l, 1, 1000000LL); return 0; }
int pthread_spin_trylock(pthread_spinlock_t *l) { return __builtin(atomic_cmpxchg32, l, 0, 1) == 0 ? 0 : EBUSY; }
int pthread_spin_unlock(pthread_spinlock_t *l) { __builtin(atomic_store32, l, 0); __builtin(atomic_notify, l, 1); return 0; }
  