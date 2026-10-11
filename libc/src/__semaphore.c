#include <semaphore.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
/* A negative word names a broker object. Nonnegative words retain the old
   unnamed semaphore representation and atomic operations. The local list
   provides POSIX repeated-open pointer identity without expanding sem_t. */
struct named_sem { sem_t sem; unsigned refs; struct named_sem *next; };
extern mode_t __umask_get(void);
static struct named_sem *named;
extern __lock_t __named_sem_lock;
__import int __sem_open(const char *name,int flags,int mode,unsigned value);
sem_t *sem_open(const char *name,int flags,...){
  unsigned value=0;int mode=0;
  if(flags&O_CREAT){
    va_list ap;va_start(ap,flags);mode=va_arg(ap,int);value=va_arg(ap,unsigned);va_end(ap);
    mode&=~__umask_get();
  }
  struct named_sem *fresh=malloc(sizeof *fresh);
  if(!fresh){errno=ENOMEM;return SEM_FAILED;}
  __lock_acquire(&__named_sem_lock);
  int id=__sem_open(name,flags,mode,value);
  if(id<0){__lock_release(&__named_sem_lock);free(fresh);return SEM_FAILED;}
  for(struct named_sem *p=named;p;p=p->next)if(p->sem.value==-id){
    p->refs++;__lock_release(&__named_sem_lock);free(fresh);return &p->sem;
  }
  fresh->sem.value=-id;fresh->refs=1;fresh->next=named;named=fresh;
  __lock_release(&__named_sem_lock);return &fresh->sem;
}
int sem_close(sem_t *s){
  __lock_acquire(&__named_sem_lock);
  struct named_sem **link=&named;
  while(*link && &(*link)->sem!=s)link=&(*link)->next;
  if(!*link){__lock_release(&__named_sem_lock);errno=EINVAL;return -1;}
  struct named_sem *p=*link;
  if(__sem_op(-p->sem.value,1,0,0)<0){__lock_release(&__named_sem_lock);return -1;}
  if(--p->refs==0){*link=p->next;free(p);}
  __lock_release(&__named_sem_lock);return 0;
}
