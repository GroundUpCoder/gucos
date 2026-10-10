"""Explicit runtime facades for the WASI libc header profile."""
API = {
    '__wasm_posix_pthread_create': {'return':'int','params':['unsigned long *','const void *','void *(*)(void *)','void *'],'variadic':False},
    '__wasm_posix_setlocale': {'return':'char *','params':['int','const char *'],'variadic':False},
    '__errno_location': {'return':'int *','params':[],'variadic':False},
    '__wasm_posix_assert': {'return':'void','params':['const char *','const char *','int','const char *'],'variadic':False},
    **{f'__wasm_posix_{stream}': {'return':'struct FILE *','params':[],'variadic':False}
       for stream in ['stdin','stdout','stderr']},
}
BRIDGES = {
    '__wasm_posix_pthread_create': ('''#include <pthread.h>
#include <errno.h>
int __wasm_posix_pthread_create(unsigned long *thread,const void *attributes,void *(*start)(void *),void *arg){
 /* WASI's opaque attribute layout is not the destination layout. The pinned
    libc++ std::thread implementation uses default attributes. */
 if(attributes)return ENOTSUP;
 return pthread_create(thread,0,start,arg);
}
''',{'pthread_create':'int pthread_create(unsigned long *, const void *, void *(*)(void *), void *)'}),
    '__wasm_posix_setlocale': ('''#include <locale.h>
char *__wasm_posix_setlocale(int category,const char *name){
 switch(category){
 case 0:return setlocale(LC_CTYPE,name);
 case 1:return setlocale(LC_NUMERIC,name);
 case 2:return setlocale(LC_TIME,name);
 case 3:return setlocale(LC_COLLATE,name);
 case 4:return setlocale(LC_MONETARY,name);
 case 6:return setlocale(LC_ALL,name);
 default:return 0;
 }
}
''', {'setlocale':'char * setlocale(int, const char *)'}),
    '__wasm_posix_assert': ('#include <assert.h>\nvoid __wasm_posix_assert(const char *expr,const char *file,int line,const char *function){__assert_fail(expr,file,line);}\n',
                          {'__assert_fail':'void __assert_fail(const char *, const char *, int)'}),
    **{f'__wasm_posix_{stream}': (f'#include <stdio.h>\nFILE *__wasm_posix_{stream}(void){{return {stream};}}\n',
                                {f'__{stream}_file':f'extern struct FILE __{stream}_file;'})
       for stream in ['stdin','stdout','stderr']},
}

# The facade IDs are private to wasi-libc.hpp; validate before calling libc.
for name in ['clock_gettime', 'clock_getres']:
    facade = '__wasm_posix_' + name
    API[facade] = {'return':'int','params':['const void *','void *'],'variadic':False}
    BRIDGES[facade] = ('''#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
_Static_assert(sizeof(struct timespec)==16 && _Alignof(struct timespec)==8 &&
 offsetof(struct timespec,tv_sec)==0 && offsetof(struct timespec,tv_nsec)==8,
 "WASI timespec layout");
int '''+facade+'''(const void *id,void *output){
 int clock;
 switch((uintptr_t)id){
 case 1:clock=CLOCK_REALTIME;break;
 case 2:clock=CLOCK_MONOTONIC;break;
 default:errno=EINVAL;return -1;
 }
 '''+('if(!output){errno=EFAULT;return -1;}\n' if name=='clock_gettime' else '')+'''return '''+name+'''(clock,(struct timespec *)output);
}
''', {name:'int '+name+'(int, struct timespec *)'})

def adapt_api(api):
    for row in api.values():
        row['return']=row['return'].replace('struct _IO_FILE','struct FILE')
        row['params']=[s.replace('struct _IO_FILE','struct FILE') for s in row['params']]
    # The pinned musl-derived headers spell pthread_t as an integer in C++
    # but an opaque pointer in C. The destination uses unsigned long handles.
    # Normalize the C header view to the actual C++/destination handle ABI.
    for name,params in {
        'pthread_join':['unsigned long','void * *'],
        'pthread_detach':['unsigned long'],
        'pthread_equal':['unsigned long','unsigned long'],
    }.items():
        if name in api:api[name]={'return':'int','params':params,'variadic':False}
    if 'pthread_self' in api:api['pthread_self']={'return':'unsigned long','params':[],'variadic':False}
    if 'pthread_create' in api:api['pthread_create']['params'][0]='unsigned long *'
    for name,row in API.items():
        if name in api and api[name]!=row:
            raise ValueError('conflicting runtime adapter declaration: '+name)
        api[name]=row.copy()

# Itanium sized delete is emitted by Clang for virtual deleting destructors.
# Default allocation for the exceptions-disabled profile. A program providing
# its own operators resolves those definitions first. Handler registration and
# exception-throwing allocation are not supplied by this profile.
for name in ['_Znwm','_Znam']:
    BRIDGES[name]=('#include <stdlib.h>\nvoid *'+name+'(unsigned int n){void *p=malloc(n?n:1);if(!p)abort();return p;}\n',
                   {'malloc':'void * malloc(unsigned long)','abort':'void abort(void)'})
for name in ['_ZdlPv','_ZdaPv']:
    BRIDGES[name]=('#include <stdlib.h>\nvoid '+name+'(void *p){free(p);}\n',{'free':'void free(void *)'})
# The destination allocator's native alignment is eight bytes. Extended C++
# alignment uses the usual over-allocation/header scheme, with O(alignment)
# padding and one pointer of bookkeeping. Matching aligned deletes recover the
# allocator-owned pointer; ordinary deletes continue to use free directly.
for name in ['_ZnwmSt11align_val_t','_ZnamSt11align_val_t']:
    BRIDGES[name]=('''#include <stdlib.h>
#include <stdint.h>
void *'''+name+'''(unsigned int n,unsigned int alignment){
 if(!alignment||(alignment&(alignment-1)))abort();
 if(alignment<sizeof(void *))alignment=sizeof(void *);
 if(!n)n=1;
 if(alignment>0xffffffffUL-sizeof(void *)||n>0xffffffffUL-alignment-sizeof(void *))abort();
 void *base=malloc(n+alignment-1+sizeof(void *));if(!base)abort();
 uintptr_t address=((uintptr_t)base+sizeof(void *)+alignment-1)&~(alignment-1);
 ((void **)address)[-1]=base;return (void *)address;
}
''',{'malloc':'void * malloc(unsigned long)','abort':'void abort(void)'})
for name,params in [('_ZdlPvSt11align_val_t','void *p,unsigned int alignment'),
                    ('_ZdaPvSt11align_val_t','void *p,unsigned int alignment'),
                    ('_ZdlPvmSt11align_val_t','void *p,unsigned int size,unsigned int alignment'),
                    ('_ZdaPvmSt11align_val_t','void *p,unsigned int size,unsigned int alignment')]:
    BRIDGES[name]=('#include <stdlib.h>\nvoid '+name+'('+params+'){if(p)free(((void **)p)[-1]);}\n',{'free':'void free(void *)'})
# The size is advisory; the destination allocator owns the actual block size.
BRIDGES['_ZdlPvm'] = ('#include <stdlib.h>\nvoid _ZdlPvm(void *ptr,unsigned int size){free(ptr);}\n',
                      {'free':'void free(void *)'})

# Default sized array deletion delegates to unsized array deletion, preserving
# a program's replacement operator. The compiler handles array cookies and
# destructor order before calling either deallocation function.
BRIDGES['_ZdaPvm'] = ('extern void _ZdaPv(void *);\nvoid _ZdaPvm(void *ptr,unsigned int size){_ZdaPv(ptr); }\n',
                      {'_ZdaPv':'void _ZdaPv(void *)'})

# Pinned WASI libc++ ABI: std::__2::mutex contains one 24-byte, 4-aligned
# pthread mutex initialized to all zero bytes. Its opaque storage can contain
# the destination's 16-byte default mutex directly; no allocation or no-op lock.
# Only these std::mutex operations use this facade. This does not adapt arbitrary
# WASI pthread attributes or recursive-mutex constants.
_MUTEX_PREFIX = '''#include <pthread.h>
#include <stdlib.h>
_Static_assert(sizeof(pthread_mutex_t)==16, "update libc++ mutex storage adapter");
_Static_assert(_Alignof(pthread_mutex_t)<=4, "update libc++ mutex alignment adapter");
'''
for suffix, body, imports in [
    ('4lockEv', 'void {name}(void *p){if(pthread_mutex_lock((pthread_mutex_t *)p))abort();}',
     {'pthread_mutex_lock':'int pthread_mutex_lock(void *)','abort':'void abort(void)'}),
    ('8try_lockEv', '_Bool {name}(void *p){return pthread_mutex_trylock((pthread_mutex_t *)p)==0;}',
     {'pthread_mutex_trylock':'int pthread_mutex_trylock(void *)'}),
    ('6unlockEv', 'void {name}(void *p){if(pthread_mutex_unlock((pthread_mutex_t *)p))abort();}',
     {'pthread_mutex_unlock':'int pthread_mutex_unlock(void *)','abort':'void abort(void)'}),
    ('D1Ev', 'void *{name}(void *p){pthread_mutex_destroy((pthread_mutex_t *)p);return p;}',
     {'pthread_mutex_destroy':'int pthread_mutex_destroy(void *)'}),
    ('D2Ev', 'void *{name}(void *p){pthread_mutex_destroy((pthread_mutex_t *)p);return p;}',
     {'pthread_mutex_destroy':'int pthread_mutex_destroy(void *)'}),
]:
    name='_ZNSt3__25mutex'+suffix
    BRIDGES[name]=(_MUTEX_PREFIX+body.replace('{name}',name)+'\n',imports)

from thread_runtime import BRIDGES as THREAD_BRIDGES, LAYOUTS
BRIDGES.update(THREAD_BRIDGES)
LAYOUTS.update({name:(24,) for name in BRIDGES if name.startswith('_ZNSt3__25mutex')})

if __name__ == '__main__':
    import json
    print(json.dumps({name:source for name,(source,_) in BRIDGES.items()}))
