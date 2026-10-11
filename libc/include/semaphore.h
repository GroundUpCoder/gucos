
#pragma once
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <__thread.h>
typedef struct { int value; } sem_t;
#define SEM_VALUE_MAX INT_MAX
static inline int sem_init(sem_t *s,int shared,unsigned value){if(shared){errno=ENOSYS;return -1;}if(value>SEM_VALUE_MAX){errno=EINVAL;return -1;}s->value=value;return 0;}
static inline int sem_destroy(sem_t *s){(void)s;return 0;}
static inline int sem_post(sem_t *s){int v;do{v=__builtin(atomic_load32,&s->value);if(v==SEM_VALUE_MAX){errno=EOVERFLOW;return -1;}}while(__builtin(atomic_cmpxchg32,&s->value,v,v+1)!=v);__builtin(atomic_notify,&s->value,1);return 0;}
static inline int sem_trywait(sem_t *s){int v;do{v=__builtin(atomic_load32,&s->value);if(v==0){errno=EAGAIN;return -1;}}while(__builtin(atomic_cmpxchg32,&s->value,v,v-1)!=v);return 0;}
static inline int sem_wait(sem_t *s){while(sem_trywait(s))__thread_wait(&s->value,0,-1LL);return 0;}
static inline int sem_getvalue(sem_t *s,int *v){*v=__builtin(atomic_load32,&s->value);return 0;}
  