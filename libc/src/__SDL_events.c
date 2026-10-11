
/* Timers run on real POSIX workers, never by reentering a suspended C frame.
   The callback owns its application userdata; cancellation prevents repeats.
   Each live timer owns a detached worker and waits without busy polling. */
#include <SDL.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
static pthread_mutex_t timer_lock=PTHREAD_MUTEX_INITIALIZER;
typedef struct Timer {struct Timer *next;SDL_TimerID id;Uint32 interval;SDL_TimerCallback callback;void *userdata;bool canceled;pthread_cond_t wake;} Timer;
static Timer *timers;
static Uint32 timer_serial;
static void *timer_worker(void *arg){Timer *t=arg;pthread_mutex_lock(&timer_lock);while(!t->canceled){struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_sec+=t->interval/1000;until.tv_nsec+=(long)(t->interval%1000)*1000000L;if(until.tv_nsec>=1000000000L){until.tv_sec++;until.tv_nsec-=1000000000L;}int status=0;while(!t->canceled&&status!=ETIMEDOUT){status=pthread_cond_timedwait(&t->wake,&timer_lock,&until);if(status&&status!=ETIMEDOUT){t->canceled=1;break;}}if(t->canceled)break;pthread_mutex_unlock(&timer_lock);Uint32 next=t->callback(t->userdata,t->id,t->interval);pthread_mutex_lock(&timer_lock);if(!next)break;t->interval=next;}Timer **p=&timers;while(*p&&*p!=t)p=&(*p)->next;if(*p)*p=t->next;pthread_mutex_unlock(&timer_lock);pthread_cond_destroy(&t->wake);free(t);return NULL;}
SDL_TimerID SDL_AddTimer(Uint32 interval,SDL_TimerCallback callback,void *userdata){if(!callback){SDL_SetError("Invalid timer callback");return 0;}Timer *t=calloc(1,sizeof(*t));if(!t){SDL_SetError("Out of memory");return 0;}pthread_cond_init(&t->wake,NULL);t->interval=interval;t->callback=callback;t->userdata=userdata;pthread_mutex_lock(&timer_lock);if(timer_serial==0xffffffffu){pthread_mutex_unlock(&timer_lock);pthread_cond_destroy(&t->wake);free(t);SDL_SetError("Timer IDs exhausted");return 0;}t->id=++timer_serial;t->next=timers;timers=t;pthread_t thread;pthread_attr_t attr;pthread_attr_init(&attr);pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);int rc=pthread_create(&thread,&attr,timer_worker,t);pthread_attr_destroy(&attr);SDL_TimerID id=t->id;if(rc){timers=t->next;pthread_cond_destroy(&t->wake);free(t);id=0;SDL_SetError("Timer worker creation failed: %d",rc);}pthread_mutex_unlock(&timer_lock);return id;}
bool SDL_RemoveTimer(SDL_TimerID id){bool found=0;pthread_mutex_lock(&timer_lock);for(Timer *t=timers;t;t=t->next){if(t->id==id&&!t->canceled){t->canceled=1;pthread_cond_signal(&t->wake);found=1;break;}}pthread_mutex_unlock(&timer_lock);if(!found)SDL_SetError("Timer not found");return found;}
int __sdl_timer_wait_bound(int timeout){pthread_mutex_lock(&timer_lock);bool active=0;for(Timer *t=timers;t;t=t->next)if(!t->canceled){active=1;break;}pthread_mutex_unlock(&timer_lock);return active&&timeout>10?10:timeout;}
void __sdl_cancel_timers(void){pthread_mutex_lock(&timer_lock);for(Timer *t=timers;t;t=t->next){t->canceled=1;pthread_cond_signal(&t->wake);}pthread_mutex_unlock(&timer_lock);}
bool SDL_TryLockSpinlock(SDL_SpinLock *lock){return __builtin(atomic_cmpxchg32,lock,0,1)==0;}
void SDL_LockSpinlock(SDL_SpinLock *lock){while(!SDL_TryLockSpinlock(lock))sched_yield();}
void SDL_UnlockSpinlock(SDL_SpinLock *lock){__builtin(atomic_store32,lock,0);}
