"""Pinned libc++ thread/condition-variable ABI over destination pthreads.

Future/promise runtime entry points are intentionally absent: unsupported uses
must fail linking. The private thread state implements exit notifications.
"""
PREFIX = '''#ifndef LLC_THREAD_RUNTIME_LAYOUT
#define LLC_THREAD_RUNTIME_LAYOUT
#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
_Static_assert(sizeof(void *)==4 && sizeof(pthread_t)==4 && sizeof(pthread_key_t)==4, "thread ABI changed");
_Static_assert(sizeof(pthread_cond_t)<=48 && _Alignof(pthread_cond_t)<=4, "condition variable ABI changed");
struct llc_unique_lock {void *mutex; _Bool owns;};
_Static_assert(offsetof(struct llc_unique_lock,owns)==4, "unique_lock ABI changed");
struct llc_notification {void *condition;void *mutex;};
struct llc_thread_state {struct llc_notification *rows;unsigned size,capacity;};
void *_ZNSt3__215__thread_structC1Ev(void *);
void *_ZNSt3__215__thread_structD1Ev(void *);
void *_ZNSt3__219__thread_local_dataEv(void);
void _ZNSt3__215__thread_struct25notify_all_at_thread_exitEPNS_18condition_variableEPNS_5mutexE(void *,void *,void *);
#endif
'''
BRIDGES={}
LAYOUTS={}
def add(name,body,imports,layouts=()):
    BRIDGES[name]=(PREFIX+body+'\n',imports)
    if layouts:LAYOUTS[name]=layouts

# Default exception-runtime termination. Custom terminate-handler installation
# is not provided by this profile and therefore remains a link-time failure.
add('_ZSt9terminatev','void _ZSt9terminatev(void){abort();}',{'abort':'void abort(void)'})

for suffix,body,imports in [
    ('10notify_allEv','void {name}(void *p){pthread_cond_broadcast(p);}',{'pthread_cond_broadcast':'int pthread_cond_broadcast(void *)'}),
    ('10notify_oneEv','void {name}(void *p){pthread_cond_signal(p);}',{'pthread_cond_signal':'int pthread_cond_signal(void *)'}),
    ('D1Ev','void *{name}(void *p){pthread_cond_destroy(p);return p;}',{'pthread_cond_destroy':'int pthread_cond_destroy(void *)'}),
    ('D2Ev','void *{name}(void *p){pthread_cond_destroy(p);return p;}',{'pthread_cond_destroy':'int pthread_cond_destroy(void *)'}),
]:
    name='_ZNSt3__218condition_variable'+suffix
    add(name,body.replace('{name}',name),imports,(48,))
add('_ZNSt3__218condition_variable4waitERNS_11unique_lockINS_5mutexEEE',
    'void _ZNSt3__218condition_variable4waitERNS_11unique_lockINS_5mutexEEE(void *p,void *lock){struct llc_unique_lock *l=lock;if(!l->owns||pthread_cond_wait(p,l->mutex))abort();}',
    {'pthread_cond_wait':'int pthread_cond_wait(void *, void *)','abort':'void abort(void)'},(48,5))
for operation in ['join','detach']:
    name='_ZNSt3__26thread'+str(len(operation))+operation+'Ev'
    call='pthread_join(*t,0)' if operation=='join' else 'pthread_detach(*t)'
    declaration='int pthread_join(unsigned long, void * *)' if operation=='join' else 'int pthread_detach(unsigned long)'
    add(name,f'void {name}(void *p){{pthread_t *t=p;if(!*t||{call})abort();*t=0;}}',
        {'pthread_'+operation:declaration,'abort':'void abort(void)'},(4,))
for suffix in ['D1Ev','D2Ev']:
    name='_ZNSt3__26thread'+suffix
    add(name,f'void *{name}(void *p){{if(*(pthread_t *)p)abort();return p;}}',{'abort':'void abort(void)'},(4,))
for suffix in ['C1Ev','C2Ev']:
    name='_ZNSt3__215__thread_struct'+suffix
    add(name,f'void *{name}(void *p){{*(void **)p=calloc(1,sizeof(struct llc_thread_state));if(!*(void **)p)abort();return p;}}',
        {'calloc':'void * calloc(unsigned long, unsigned long)','abort':'void abort(void)'},(4,))
for suffix in ['D1Ev','D2Ev']:
    name='_ZNSt3__215__thread_struct'+suffix
    add(name,f'''void *{name}(void *p){{struct llc_thread_state *s=*(void **)p;
for(unsigned i=0;i<s->size;i++){{pthread_cond_broadcast(s->rows[i].condition);if(pthread_mutex_unlock(s->rows[i].mutex))abort();}}
free(s->rows);free(s);return p;}}''',
        {'pthread_cond_broadcast':'int pthread_cond_broadcast(void *)','pthread_mutex_unlock':'int pthread_mutex_unlock(void *)','abort':'void abort(void)','free':'void free(void *)'},(4,))
notify='_ZNSt3__215__thread_struct25notify_all_at_thread_exitEPNS_18condition_variableEPNS_5mutexE'
add(notify,f'''void {notify}(void *p,void *condition,void *mutex){{
struct llc_thread_state *s=*(void **)p;
if(s->size==s->capacity){{unsigned capacity=s->capacity?s->capacity*2:1;if(capacity<s->capacity||capacity>0xffffffffU/sizeof(*s->rows))abort();
void *rows=realloc(s->rows,capacity*sizeof(*s->rows));if(!rows)abort();s->rows=rows;s->capacity=capacity;}}
s->rows[s->size].condition=condition;s->rows[s->size].mutex=mutex;s->size++;
}}''',{'realloc':'void * realloc(void *, unsigned long)','abort':'void abort(void)'},(4,None,None))
add('_ZNSt3__219__thread_local_dataEv','''
static pthread_once_t once=PTHREAD_ONCE_INIT;
static pthread_key_t key;
static void destroy(void *p){_ZNSt3__215__thread_structD1Ev(p);free(p);}
static void initialize(void){if(pthread_key_create(&key,destroy))abort();}
void *_ZNSt3__219__thread_local_dataEv(void){if(pthread_once(&once,initialize))abort();return &key;}
''',{'_ZNSt3__215__thread_structD1Ev':'void * _ZNSt3__215__thread_structD1Ev(void *)',
     'pthread_key_create':'int pthread_key_create(unsigned int *, void (*)(void *))',
     'pthread_once':'int pthread_once(int *, void (*)(void))','free':'void free(void *)','abort':'void abort(void)'})
name='_ZNSt3__225notify_all_at_thread_exitERNS_18condition_variableENS_11unique_lockINS_5mutexEEE'
add(name,f'''void {name}(void *condition,void *lock){{
struct llc_unique_lock *l=lock;if(!l->owns)abort();
pthread_key_t key=*(pthread_key_t *)_ZNSt3__219__thread_local_dataEv();void *state=pthread_getspecific(key);
if(!state){{state=malloc(4);if(!state)abort();_ZNSt3__215__thread_structC1Ev(state);if(pthread_setspecific(key,state))abort();}}
{notify}(state,condition,l->mutex);l->owns=0;l->mutex=0;
}}''',{'_ZNSt3__219__thread_local_dataEv':'void * _ZNSt3__219__thread_local_dataEv(void)',
       '_ZNSt3__215__thread_structC1Ev':'void * _ZNSt3__215__thread_structC1Ev(void *)',
       notify:f'void {notify}(void *, void *, void *)','malloc':'void * malloc(unsigned long)',
       'pthread_getspecific':'void * pthread_getspecific(unsigned int)',
       'pthread_setspecific':'int pthread_setspecific(unsigned int, const void *)','abort':'void abort(void)'})
# Exception-disabled libc++ error paths report the error and terminate.
add('_ZNSt3__220__throw_system_errorEiPKc','''#include <stdio.h>
#include <string.h>
void _ZNSt3__220__throw_system_errorEiPKc(unsigned int code,void *message){fprintf(stderr,"%s: %s\\n",(const char *)message,strerror((int)code));abort();}
''',{'fprintf':'int fprintf(struct FILE *, const char *, ...)','strerror':'char * strerror(int)',
     '__stderr_file':'extern struct FILE __stderr_file;','abort':'void abort(void)'})
add('_ZNSt3__222__libcpp_verbose_abortEPKcz','''#include <stdio.h>
#include <stdarg.h>
void _ZNSt3__222__libcpp_verbose_abortEPKcz(void *message,...){va_list args;va_start(args,message);vfprintf(stderr,(const char *)message,args);va_end(args);abort();}
''',{'vfprintf':'int vfprintf(struct FILE *, const char *, int * *)',
     '__stderr_file':'extern struct FILE __stderr_file;','abort':'void abort(void)'})
