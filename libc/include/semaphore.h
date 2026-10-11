
#pragma once
__require_source("__semaphore.c");
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <__thread.h>
#ifdef __SEM_IMPLEMENTATION
#define __SEM_INLINE
#else
#define __SEM_INLINE static inline
#endif
typedef struct { int value; } sem_t;
#define SEM_VALUE_MAX INT_MAX
#define SEM_FAILED ((sem_t *)0)
sem_t *sem_open(const char *name,int flags,...);
int sem_close(sem_t *s);
__import int __sem_unlink(const char *name);
__SEM_INLINE int sem_unlink(const char *name){return __sem_unlink(name);}
__import int __sem_op(int id,int action,long long sec,int nsec);
__SEM_INLINE int sem_init(sem_t *s,int shared,unsigned value){if(shared){errno=ENOSYS;return -1;}if(value>SEM_VALUE_MAX){errno=EINVAL;return -1;}s->value=value;return 0;}
__SEM_INLINE int sem_destroy(sem_t *s){(void)s;return 0;}
__SEM_INLINE int sem_post(sem_t *s){if(s->value<0)return __sem_op(-s->value,3,0,0);int v;do{v=__builtin(atomic_load32,&s->value);if(v==SEM_VALUE_MAX){errno=EOVERFLOW;return -1;}}while(__builtin(atomic_cmpxchg32,&s->value,v,v+1)!=v);__builtin(atomic_notify,&s->value,1);return 0;}
__SEM_INLINE int sem_trywait(sem_t *s){if(s->value<0)return __sem_op(-s->value,5,0,0);int v;do{v=__builtin(atomic_load32,&s->value);if(v==0){errno=EAGAIN;return -1;}}while(__builtin(atomic_cmpxchg32,&s->value,v,v-1)!=v);return 0;}
__SEM_INLINE int sem_wait(sem_t *s){if(s->value<0)return __sem_op(-s->value,4,0,0);while(sem_trywait(s))__thread_wait(&s->value,0,-1LL);return 0;}
__SEM_INLINE int sem_getvalue(sem_t *s,int *v){if(s->value<0){int n=__sem_op(-s->value,7,0,0);if(n<0)return -1;*v=n;return 0;}*v=__builtin(atomic_load32,&s->value);return 0;}

/* Absolute CLOCK_REALTIME deadline; an immediately available token wins even
   with an expired deadline. The original unnamed atomic paths stay intact. */
__SEM_INLINE int sem_timedwait(sem_t *s,const struct timespec *deadline){
  if(!sem_trywait(s))return 0;
  if(errno!=EAGAIN)return -1;
  if(deadline->tv_nsec<0||deadline->tv_nsec>=1000000000L){errno=EINVAL;return -1;}
  if(s->value<0)return __sem_op(-s->value,6,deadline->tv_sec,deadline->tv_nsec);
  for(;;){
    struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now))return -1;
    if(deadline->tv_sec<now.tv_sec){errno=ETIMEDOUT;return -1;}
    long long seconds=deadline->tv_sec-now.tv_sec;
    long long nanos=(long long)deadline->tv_nsec-now.tv_nsec;
    if(seconds<0||(seconds==0&&nanos<=0)){errno=ETIMEDOUT;return -1;}
    /* Bound each wait to recheck wall time, and avoid duration overflow. */
    long long timeout=seconds>1?1000000000LL:seconds*1000000000LL+nanos;
    __thread_wait(&s->value,0,timeout);
    if(!sem_trywait(s))return 0;
    if(errno!=EAGAIN)return -1;
  }
}

#undef __SEM_INLINE
