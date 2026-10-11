
/* SDL foundation for the wasm POSIX backend. Locks use the runtime's real
   shared-memory pthread implementation; streams delegate to libc or callbacks. */
#include <SDL.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
struct SDL_Mutex { pthread_mutex_t lock; };
struct SDL_RWLock { pthread_rwlock_t lock; };
SDL_Mutex *SDL_CreateMutex(void) { SDL_Mutex *m=calloc(1,sizeof(*m)); pthread_mutexattr_t a; if(!m){SDL_SetError("Out of memory");return NULL;} pthread_mutexattr_init(&a);pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);if(pthread_mutex_init(&m->lock,&a)){free(m);m=NULL;SDL_SetError("Mutex initialization failed");}pthread_mutexattr_destroy(&a);return m; }
void SDL_DestroyMutex(SDL_Mutex *m){if(m){pthread_mutex_destroy(&m->lock);free(m);}}
void SDL_LockMutex(SDL_Mutex *m){if(m)pthread_mutex_lock(&m->lock);}
bool SDL_TryLockMutex(SDL_Mutex *m){return !m||pthread_mutex_trylock(&m->lock)==0;}
void SDL_UnlockMutex(SDL_Mutex *m){if(m)pthread_mutex_unlock(&m->lock);}
SDL_RWLock *SDL_CreateRWLock(void){SDL_RWLock *m=calloc(1,sizeof(*m));if(!m){SDL_SetError("Out of memory");return NULL;}if(pthread_rwlock_init(&m->lock,NULL)){free(m);SDL_SetError("RWLock initialization failed");return NULL;}return m;}
void SDL_DestroyRWLock(SDL_RWLock *m){if(m){pthread_rwlock_destroy(&m->lock);free(m);}}
void SDL_LockRWLockForReading(SDL_RWLock *m){if(m)pthread_rwlock_rdlock(&m->lock);}
void SDL_LockRWLockForWriting(SDL_RWLock *m){if(m)pthread_rwlock_wrlock(&m->lock);}
void SDL_UnlockRWLock(SDL_RWLock *m){if(m)pthread_rwlock_unlock(&m->lock);}
int SDL_GetAtomicInt(SDL_AtomicInt *a){return __builtin(atomic_load32,&a->value);}
int SDL_AddAtomicInt(SDL_AtomicInt *a,int n){return __builtin(atomic_add32,&a->value,n);}
int SDL_SetAtomicInt(SDL_AtomicInt *a,int n){return __builtin(atomic_xchg32,&a->value,n);}
bool SDL_CompareAndSwapAtomicInt(SDL_AtomicInt *a,int old,int n){return __builtin(atomic_cmpxchg32,&a->value,old,n)==old;}
bool SDL_ShouldInit(SDL_InitState *s){while(SDL_GetAtomicInt(&s->status)!=SDL_INIT_STATUS_INITIALIZED){if(SDL_CompareAndSwapAtomicInt(&s->status,0,1)){s->thread=pthread_self();return 1;}sched_yield();}return 0;}
bool SDL_ShouldQuit(SDL_InitState *s){while(SDL_GetAtomicInt(&s->status)!=SDL_INIT_STATUS_UNINITIALIZED){if(SDL_CompareAndSwapAtomicInt(&s->status,2,3)){s->thread=pthread_self();return 1;}sched_yield();}return 0;}
void SDL_SetInitialized(SDL_InitState *s,bool b){assert(s->thread==pthread_self());SDL_SetAtomicInt(&s->status,b?2:0);}
/* Property IDs are monotonic handles, never pointers or recycled live IDs.
   A recursive registry mutex also serializes callbacks and explicit locking. */
typedef struct Prop { struct Prop *next;char *name;char *cached;SDL_PropertyType type;void *ptr;Sint64 number;float real;SDL_CleanupPropertyCallback cleanup;void *userdata; } Prop;
typedef struct Props {struct Props *next;SDL_PropertiesID id;Prop *head;} Props;
static pthread_mutex_t prop_lock=PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
static Props *prop_sets;
static Uint32 prop_serial;
static Props *props_find(SDL_PropertiesID id){Props *p;for(p=prop_sets;p;p=p->next)if(p->id==id)return p;return NULL;}
static Prop *prop_find(Props *p,const char *name){Prop *v;if(!p||!name)return NULL;for(v=p->head;v;v=v->next)if(!strcmp(v->name,name))return v;return NULL;}
static void prop_free(Prop *v){if(v->cleanup)v->cleanup(v->userdata,v->ptr);if(v->type==SDL_PROPERTY_TYPE_STRING)free(v->ptr);free(v->name);free(v->cached);free(v);}
SDL_PropertiesID SDL_CreateProperties(void){Props *p=calloc(1,sizeof(*p));if(!p){SDL_SetError("Out of memory");return 0;}pthread_mutex_lock(&prop_lock);if(prop_serial==0xffffffffu){pthread_mutex_unlock(&prop_lock);free(p);SDL_SetError("Property IDs exhausted");return 0;}p->id=++prop_serial;p->next=prop_sets;prop_sets=p;pthread_mutex_unlock(&prop_lock);return p->id;}
bool SDL_LockProperties(SDL_PropertiesID id){pthread_mutex_lock(&prop_lock);if(!props_find(id)){pthread_mutex_unlock(&prop_lock);return SDL_SetError("Invalid properties");}return 1;}
void SDL_UnlockProperties(SDL_PropertiesID id){(void)id;pthread_mutex_unlock(&prop_lock);}
void SDL_DestroyProperties(SDL_PropertiesID id){Props **p,*s;pthread_mutex_lock(&prop_lock);for(p=&prop_sets;*p&&(*p)->id!=id;p=&(*p)->next){}s=*p;if(s){*p=s->next;while(s->head){Prop *v=s->head;s->head=v->next;prop_free(v);}free(s);}pthread_mutex_unlock(&prop_lock);}
bool SDL_ClearProperty(SDL_PropertiesID id,const char *name){Props *s;Prop **p;if(!name||!*name)return SDL_SetError("Invalid property name");if(!SDL_LockProperties(id))return 0;s=props_find(id);for(p=&s->head;*p&&strcmp((*p)->name,name);p=&(*p)->next){}if(*p){Prop *v=*p;*p=v->next;prop_free(v);}SDL_UnlockProperties(id);return 1;}
SDL_PropertyType SDL_GetPropertyType(SDL_PropertiesID id,const char *name){SDL_PropertyType t=SDL_PROPERTY_TYPE_INVALID;pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);if(p)t=p->type;pthread_mutex_unlock(&prop_lock);return t;}
bool SDL_HasProperty(SDL_PropertiesID id,const char *name){return SDL_GetPropertyType(id,name)!=SDL_PROPERTY_TYPE_INVALID;}
/* Copy from a snapshot: replacement cleanup callbacks may edit either group. */
bool SDL_CopyProperties(SDL_PropertiesID src,SDL_PropertiesID dst){
    Prop *snapshot=NULL,*tail=NULL;
    if(!SDL_LockProperties(src))return 0;
    if(!props_find(dst)){SDL_UnlockProperties(src);return SDL_SetError("Invalid destination properties");}
    if(src==dst){SDL_UnlockProperties(src);return 1;}
    bool ok=1;
    for(Prop *p=props_find(src)->head;p;p=p->next){
        if(p->cleanup)continue;
        Prop *q=calloc(1,sizeof(*q));
        if(!q){ok=SDL_SetError("Out of memory");break;}
        q->name=strdup(p->name);q->type=p->type;q->number=p->number;q->real=p->real;
        q->ptr=p->type==SDL_PROPERTY_TYPE_STRING?strdup(p->ptr):p->ptr;
        if(!q->name||(p->type==SDL_PROPERTY_TYPE_STRING&&!q->ptr)){prop_free(q);ok=SDL_SetError("Out of memory");break;}
        if(tail)tail->next=q;else snapshot=q;tail=q;
    }
    for(Prop *p=snapshot;ok&&p;p=p->next){
        switch(p->type){
        case SDL_PROPERTY_TYPE_POINTER:ok=SDL_SetPointerProperty(dst,p->name,p->ptr);break;
        case SDL_PROPERTY_TYPE_STRING:ok=SDL_SetStringProperty(dst,p->name,p->ptr);break;
        case SDL_PROPERTY_TYPE_NUMBER:ok=SDL_SetNumberProperty(dst,p->name,p->number);break;
        case SDL_PROPERTY_TYPE_FLOAT:ok=SDL_SetFloatProperty(dst,p->name,p->real);break;
        case SDL_PROPERTY_TYPE_BOOLEAN:ok=SDL_SetBooleanProperty(dst,p->name,p->number!=0);break;
        default:break;
        }
    }
    while(snapshot){Prop *p=snapshot;snapshot=p->next;prop_free(p);}
    SDL_UnlockProperties(src);return ok;
}
bool SDL_EnumerateProperties(SDL_PropertiesID id,SDL_EnumeratePropertiesCallback callback,void *userdata){
    if(!callback)return SDL_InvalidParamError("callback");
    if(!SDL_LockProperties(id))return 0;
    size_t count=0;for(Prop*p=props_find(id)->head;p;p=p->next)count++;
    char **names=count?calloc(count,sizeof(*names)):NULL;
    if(count&&!names){SDL_UnlockProperties(id);return SDL_SetError("Out of memory");}
    size_t n=0;bool ok=1;
    for(Prop*p=props_find(id)->head;p;p=p->next){names[n]=strdup(p->name);if(!names[n]){ok=SDL_SetError("Out of memory");break;}n++;}
    /* Hold the recursive lock across callbacks, but never borrow list nodes or
       names: callbacks can clear, replace, or add properties safely. */
    for(size_t i=0;ok&&i<n;i++){
        Props *set=props_find(id);
        if(!set){ok=SDL_SetError("Properties destroyed during enumeration");break;}
        if(prop_find(set,names[i]))callback(userdata,id,names[i]);
    }
    for(size_t i=0;i<n;i++)free(names[i]);free(names);
    SDL_UnlockProperties(id);return ok;
}

static bool prop_set(SDL_PropertiesID id,const char *name,SDL_PropertyType t,void *ptr,Sint64 num,float real,SDL_CleanupPropertyCallback cleanup,void *userdata){Prop *p;Props *s;if(!name||!*name)return SDL_SetError("Invalid property name");p=calloc(1,sizeof(*p));if(!p)return SDL_SetError("Out of memory");p->name=strdup(name);if(!p->name){free(p);return SDL_SetError("Out of memory");}if(!SDL_LockProperties(id)){free(p->name);free(p);return 0;}SDL_ClearProperty(id,name);s=props_find(id);if(!s){SDL_UnlockProperties(id);free(p->name);free(p);return SDL_SetError("Properties destroyed during replacement");}p->type=t;p->ptr=ptr;p->number=num;p->real=real;p->cleanup=cleanup;p->userdata=userdata;p->next=s->head;s->head=p;SDL_UnlockProperties(id);return 1;}
bool SDL_SetPointerPropertyWithCleanup(SDL_PropertiesID id,const char *name,void *v,SDL_CleanupPropertyCallback c,void *u){if(!v){if(c)c(u,v);return SDL_ClearProperty(id,name);}bool ok=prop_set(id,name,SDL_PROPERTY_TYPE_POINTER,v,0,0,c,u);if(!ok&&c)c(u,v);return ok;}
bool SDL_SetPointerProperty(SDL_PropertiesID id,const char *name,void *v){return SDL_SetPointerPropertyWithCleanup(id,name,v,NULL,NULL);}
bool SDL_SetStringProperty(SDL_PropertiesID id,const char *name,const char *v){if(!v)return SDL_ClearProperty(id,name);char *s=strdup(v);if(!s)return SDL_SetError("Out of memory");if(!prop_set(id,name,SDL_PROPERTY_TYPE_STRING,s,0,0,NULL,NULL)){free(s);return 0;}return 1;}
bool SDL_SetNumberProperty(SDL_PropertiesID id,const char *name,Sint64 v){return prop_set(id,name,SDL_PROPERTY_TYPE_NUMBER,NULL,v,0,NULL,NULL);}
bool SDL_SetBooleanProperty(SDL_PropertiesID id,const char *name,bool v){return prop_set(id,name,SDL_PROPERTY_TYPE_BOOLEAN,NULL,v,0,NULL,NULL);}
bool SDL_SetFloatProperty(SDL_PropertiesID id,const char *name,float v){return prop_set(id,name,SDL_PROPERTY_TYPE_FLOAT,NULL,0,v,NULL,NULL);}
#define PROP_GET(fn,T,tag,field) T fn(SDL_PropertiesID id,const char *name,T fallback){pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);T v=p&&p->type==tag?(T)p->field:fallback;pthread_mutex_unlock(&prop_lock);return v;}
PROP_GET(SDL_GetPointerProperty,void *,SDL_PROPERTY_TYPE_POINTER,ptr)
const char *SDL_GetStringProperty(SDL_PropertiesID id,const char *name,const char *fallback){pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);const char *v=fallback;if(p){if(p->type==SDL_PROPERTY_TYPE_STRING)v=p->ptr;else if(p->type==SDL_PROPERTY_TYPE_BOOLEAN)v=p->number?"true":"false";else if(p->type==SDL_PROPERTY_TYPE_NUMBER||p->type==SDL_PROPERTY_TYPE_FLOAT){if(!p->cached){char b[128];if(p->type==SDL_PROPERTY_TYPE_NUMBER)snprintf(b,sizeof(b),"%lld",p->number);else snprintf(b,sizeof(b),"%f",p->real);p->cached=strdup(b);}if(p->cached)v=p->cached;}}pthread_mutex_unlock(&prop_lock);return v;}
Sint64 SDL_GetNumberProperty(SDL_PropertiesID id,const char *name,Sint64 v){pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);if(p){if(p->type==SDL_PROPERTY_TYPE_STRING)v=strtoll(p->ptr,NULL,0);else if(p->type==SDL_PROPERTY_TYPE_FLOAT)v=(Sint64)round(p->real);else if(p->type==SDL_PROPERTY_TYPE_NUMBER||p->type==SDL_PROPERTY_TYPE_BOOLEAN)v=p->number;}pthread_mutex_unlock(&prop_lock);return v;}
bool SDL_GetBooleanProperty(SDL_PropertiesID id,const char *name,bool v){pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);if(p){if(p->type==SDL_PROPERTY_TYPE_STRING){const char *s=p->ptr;if(*s)v=(*s!='0'&&strcasecmp(s,"false"));}else if(p->type==SDL_PROPERTY_TYPE_FLOAT)v=p->real!=0;else if(p->type==SDL_PROPERTY_TYPE_NUMBER||p->type==SDL_PROPERTY_TYPE_BOOLEAN)v=p->number!=0;}pthread_mutex_unlock(&prop_lock);return v;}
float SDL_GetFloatProperty(SDL_PropertiesID id,const char *name,float v){pthread_mutex_lock(&prop_lock);Prop *p=prop_find(props_find(id),name);if(p){if(p->type==SDL_PROPERTY_TYPE_STRING)v=(float)atof(p->ptr);else if(p->type==SDL_PROPERTY_TYPE_FLOAT)v=p->real;else if(p->type==SDL_PROPERTY_TYPE_NUMBER||p->type==SDL_PROPERTY_TYPE_BOOLEAN)v=(float)p->number;}pthread_mutex_unlock(&prop_lock);return v;}
struct SDL_IOStream {SDL_IOStreamInterface iface;void *userdata;SDL_IOStatus status;SDL_PropertiesID props;};
SDL_IOStream *SDL_OpenIO(const SDL_IOStreamInterface *i,void *u){if(!i||i->version<sizeof(*i)){SDL_SetError("Invalid IO interface version");return NULL;}SDL_IOStream *s=calloc(1,sizeof(*s));if(!s){SDL_SetError("Out of memory");return NULL;}s->iface=*i;s->userdata=u;return s;}
SDL_PropertiesID SDL_GetIOProperties(SDL_IOStream *s){if(!s){SDL_SetError("Invalid stream");return 0;}if(!s->props)s->props=SDL_CreateProperties();return s->props;}
SDL_IOStatus SDL_GetIOStatus(SDL_IOStream *s){return s?s->status:SDL_IO_STATUS_ERROR;}
bool SDL_CloseIO(SDL_IOStream *s){if(!s)return 1;bool ok=s->iface.close?s->iface.close(s->userdata):1;SDL_DestroyProperties(s->props);free(s);return ok;}
Sint64 SDL_GetIOSize(SDL_IOStream *s){if(!s){SDL_SetError("Invalid stream");return -1;}if(!s->iface.size){Sint64 pos=SDL_TellIO(s);if(pos<0)return -1;Sint64 n=SDL_SeekIO(s,0,SDL_IO_SEEK_END);SDL_SeekIO(s,pos,SDL_IO_SEEK_SET);return n;}return s->iface.size(s->userdata);}
Sint64 SDL_SeekIO(SDL_IOStream *s,Sint64 off,SDL_IOWhence w){if(!s||!s->iface.seek){SDL_SetError("Stream is not seekable");return -1;}return s->iface.seek(s->userdata,off,w);}
Sint64 SDL_TellIO(SDL_IOStream *s){return SDL_SeekIO(s,0,SDL_IO_SEEK_CUR);}
size_t SDL_ReadIO(SDL_IOStream *s,void *p,size_t n){if(!s)return 0;s->status=SDL_IO_STATUS_READY;if(!n)return 0;if(!p||!s->iface.read){s->status=SDL_IO_STATUS_WRITEONLY;SDL_SetError("Stream cannot read");return 0;}SDL_ClearError();size_t r=s->iface.read(s->userdata,p,n,&s->status);if(!r&&s->status==SDL_IO_STATUS_READY)s->status=*SDL_GetError()?SDL_IO_STATUS_ERROR:SDL_IO_STATUS_EOF;return r;}
size_t SDL_WriteIO(SDL_IOStream *s,const void *p,size_t n){if(!s)return 0;s->status=SDL_IO_STATUS_READY;if(!n)return 0;if(!p||!s->iface.write){s->status=SDL_IO_STATUS_READONLY;SDL_SetError("Stream cannot write");return 0;}SDL_ClearError();size_t r=s->iface.write(s->userdata,p,n,&s->status);if(!r&&s->status==SDL_IO_STATUS_READY){s->status=SDL_IO_STATUS_ERROR;SDL_SetError("Short stream write");}return r;}
bool SDL_FlushIO(SDL_IOStream *s){if(!s)return SDL_SetError("Invalid stream");s->status=SDL_IO_STATUS_READY;SDL_ClearError();bool ok=s->iface.flush?s->iface.flush(s->userdata,&s->status):1;if(!ok&&s->status==SDL_IO_STATUS_READY)s->status=SDL_IO_STATUS_ERROR;return ok;}
/* An absolute seek already determines the result. Avoid a second kernel
   round trip to ask for that same position. A tell must also preserve stdio
   read-ahead instead of discarding it with fseek(0, SEEK_CUR). */
static Sint64 file_seek(void *u,Sint64 o,SDL_IOWhence w){
 if(w==SDL_IO_SEEK_CUR&&o==0){long p=ftell(u);if(p<0)SDL_SetError("File seek failed");return p;}
 if(w<0||w>2||o<LONG_MIN||o>LONG_MAX||fseek(u,(long)o,(int)w)){SDL_SetError("File seek failed");return -1;}
 return w==SDL_IO_SEEK_SET?o:ftell(u);
}
static Sint64 file_size(void *u){long p=ftell(u);if(p<0||fseek(u,0,SEEK_END))return -1;long n=ftell(u);if(fseek(u,p,SEEK_SET))return -1;return n;}
static size_t file_read(void *u,void *p,size_t n,SDL_IOStatus *st){size_t r=fread(p,1,n,u);if(r<n){*st=ferror(u)?SDL_IO_STATUS_ERROR:SDL_IO_STATUS_EOF;if(ferror(u))SDL_SetError("File read failed");}return r;}
static size_t file_write(void *u,const void *p,size_t n,SDL_IOStatus *st){size_t r=fwrite(p,1,n,u);if(r<n){*st=SDL_IO_STATUS_ERROR;SDL_SetError("File write failed");}return r;}
static bool file_flush(void *u,SDL_IOStatus *st){if(fflush(u)){*st=SDL_IO_STATUS_ERROR;return SDL_SetError("File flush failed");}return 1;}
static bool file_close(void *u){return fclose(u)==0;}
SDL_IOStream *SDL_IOFromFile(const char *p,const char *m){if(!p||!m){SDL_SetError("Invalid file parameters");return NULL;}FILE *f=fopen(p,m);if(!f){SDL_SetError("File open failed");return NULL;}SDL_IOStreamInterface i={sizeof(i),file_size,file_seek,file_read,file_write,file_flush,file_close};SDL_IOStream *s=SDL_OpenIO(&i,f);if(!s){fclose(f);return NULL;}if(!SDL_SetPointerProperty(SDL_GetIOProperties(s),SDL_PROP_IOSTREAM_STDIO_FILE_POINTER,f)){SDL_CloseIO(s);return NULL;}return s;}
typedef struct MemIO {Uint8 *base;size_t size,pos;} MemIO;
static Sint64 mem_size(void *u){return ((MemIO *)u)->size;}
static Sint64 mem_seek(void *u,Sint64 off,SDL_IOWhence w){MemIO *m=u;Sint64 base;if(w==SDL_IO_SEEK_SET)base=0;else if(w==SDL_IO_SEEK_CUR)base=m->pos;else if(w==SDL_IO_SEEK_END)base=m->size;else{SDL_SetError("Invalid seek origin");return -1;}if(off< -base)m->pos=0;else if(off>(Sint64)m->size-base)m->pos=m->size;else m->pos=(size_t)(base+off);return m->pos;}
static size_t mem_read(void *u,void *p,size_t n,SDL_IOStatus *st){MemIO *m=u;size_t avail=m->size-m->pos;if(n>avail)n=avail;memcpy(p,m->base+m->pos,n);m->pos+=n;return n;}
static size_t mem_write(void *u,const void *p,size_t n,SDL_IOStatus *st){MemIO *m=u;size_t avail=m->size-m->pos;if(n>avail)n=avail;memcpy(m->base+m->pos,p,n);m->pos+=n;return n;}
static bool mem_close(void *u){free(u);return 1;}
static SDL_IOStream *mem_open(const void *p,size_t n,bool ro){if(!p||!n){SDL_SetError("Invalid memory stream");return NULL;}MemIO *m=calloc(1,sizeof(*m));if(!m){SDL_SetError("Out of memory");return NULL;}m->base=(Uint8 *)p;m->size=n;SDL_IOStreamInterface i={sizeof(i),mem_size,mem_seek,mem_read,ro?NULL:mem_write,NULL,mem_close};SDL_IOStream *s=SDL_OpenIO(&i,m);if(!s){free(m);return NULL;}SDL_PropertiesID props=SDL_GetIOProperties(s);if(!SDL_SetPointerProperty(props,SDL_PROP_IOSTREAM_MEMORY_POINTER,(void *)p)||!SDL_SetNumberProperty(props,SDL_PROP_IOSTREAM_MEMORY_SIZE_NUMBER,n)){SDL_CloseIO(s);return NULL;}return s;}
SDL_IOStream *SDL_IOFromMem(void *p,size_t n){return mem_open(p,n,0);}
SDL_IOStream *SDL_IOFromConstMem(const void *p,size_t n){return mem_open(p,n,1);}
static bool io_read_integer(SDL_IOStream *src,void *out,Uint8 *bytes,size_t n){
    if(!src||!out)return SDL_InvalidParamError(!src?"src":"value");
    if(SDL_ReadIO(src,bytes,n)!=n){if(!*SDL_GetError())SDL_SetError("Incomplete integer read");return 0;}return 1;
}
bool SDL_ReadU8(SDL_IOStream *src,Uint8 *out){Uint8 b;if(!io_read_integer(src,out,&b,1))return 0;*out=b;return 1;}
bool SDL_ReadS8(SDL_IOStream *src,Sint8 *out){Uint8 b;if(!io_read_integer(src,out,&b,1))return 0;*out=(Sint8)b;return 1;}
#define IO_READ_ENDIAN(bits,T,U,order,little) bool SDL_Read##T##bits##order(SDL_IOStream*src,T##int##bits*out){Uint8 b[bits/8];if(!io_read_integer(src,out,b,sizeof(b)))return 0;U##int##bits value=0;for(size_t i=0;i<sizeof(b);i++)value=(value<<8)|b[(little)?sizeof(b)-1-i:i];*out=(T##int##bits)value;return 1;}
IO_READ_ENDIAN(16,U,U,LE,1)
IO_READ_ENDIAN(16,S,U,LE,1)
IO_READ_ENDIAN(32,U,U,LE,1)
IO_READ_ENDIAN(32,S,U,LE,1)
IO_READ_ENDIAN(64,U,U,LE,1)
IO_READ_ENDIAN(64,S,U,LE,1)
IO_READ_ENDIAN(16,U,U,BE,0)
IO_READ_ENDIAN(16,S,U,BE,0)
IO_READ_ENDIAN(32,U,U,BE,0)
IO_READ_ENDIAN(32,S,U,BE,0)
IO_READ_ENDIAN(64,U,U,BE,0)
IO_READ_ENDIAN(64,S,U,BE,0)
#undef IO_READ_ENDIAN

typedef struct DynamicIO {MemIO mem;SDL_IOStream *stream;size_t capacity;} DynamicIO;
static size_t dynamic_read(void *u,void *p,size_t n,SDL_IOStatus *st){DynamicIO*d=u;if(!d->mem.size)return 0;return mem_read(&d->mem,p,n,st);}
static size_t dynamic_write(void *u,const void *p,size_t n,SDL_IOStatus *st){
    DynamicIO*d=u;MemIO*m=&d->mem;
    if(n>(size_t)-1-m->pos-1){*st=SDL_IO_STATUS_ERROR;SDL_SetError("Dynamic stream size overflow");return 0;}
    size_t end=m->pos+n;
    if(end>=d->capacity){
        Sint64 property=SDL_GetNumberProperty(SDL_GetIOProperties(d->stream),SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER,1024);
        if(property<0||property>(Sint64)((size_t)-1)){*st=SDL_IO_STATUS_ERROR;SDL_SetError("Invalid dynamic stream chunk size");return 0;}
        size_t chunk=property?(size_t)property:1024,groups=end/chunk+1;
        if(groups>(size_t)-1/chunk){*st=SDL_IO_STATUS_ERROR;SDL_SetError("Dynamic stream capacity overflow");return 0;}
        size_t capacity=groups*chunk;Uint8 *base=malloc(capacity);
        if(!base){*st=SDL_IO_STATUS_ERROR;SDL_SetError("Out of memory");return 0;}
        if(m->size)memcpy(base,m->base,m->size);
        if(!SDL_SetPointerProperty(SDL_GetIOProperties(d->stream),SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER,base)){free(base);*st=SDL_IO_STATUS_ERROR;return 0;}
        uintptr_t address=(uintptr_t)p,start=(uintptr_t)m->base;
        if(m->base&&address>=start&&address-start<m->size)p=base+(address-start);
        free(m->base);m->base=base;d->capacity=capacity;
    }
    memmove(m->base+m->pos,p,n);m->pos=end;if(m->size<end)m->size=end;
    m->base[m->size]=0;return n;
}
static bool dynamic_close(void *u){DynamicIO*d=u;void*p=SDL_GetPointerProperty(SDL_GetIOProperties(d->stream),SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER,NULL);if(p)free(p);free(d);return 1;}
SDL_IOStream *SDL_IOFromDynamicMem(void){
    DynamicIO*d=calloc(1,sizeof(*d));if(!d){SDL_SetError("Out of memory");return NULL;}
    SDL_IOStreamInterface i={sizeof(i),mem_size,mem_seek,dynamic_read,dynamic_write,NULL,dynamic_close};
    SDL_IOStream*s=SDL_OpenIO(&i,d);if(!s){free(d);return NULL;}d->stream=s;
    if(!SDL_GetIOProperties(s)){SDL_CloseIO(s);return NULL;}return s;
}
void *SDL_LoadFile_IO(SDL_IOStream *src,size_t *datasize,bool closeio){
    char *data=NULL;size_t used=0,capacity=0;bool ok=0;
    if(datasize)*datasize=0;
    if(!src){SDL_InvalidParamError("src");return NULL;}
    Sint64 hint=SDL_GetIOSize(src);
    if(hint>(Sint64)((size_t)-2)){SDL_SetError("Stream too large");goto done;}
    capacity=hint>0?(size_t)hint:1024;
    data=malloc(capacity+1);if(!data){SDL_SetError("Out of memory");goto done;}
    for(;;){
        if(used==capacity){
            if(capacity>((size_t)-2)/2){SDL_SetError("Stream too large");goto done;}
            size_t grown=capacity*2;char *p=realloc(data,grown+1);if(!p){SDL_SetError("Out of memory");goto done;}data=p;capacity=grown;
        }
        size_t n=SDL_ReadIO(src,data+used,capacity-used);
        if(n>capacity-used){SDL_SetError("Invalid stream read count");goto done;}
        used+=n;SDL_IOStatus status=SDL_GetIOStatus(src);
        if(status==SDL_IO_STATUS_ERROR||status==SDL_IO_STATUS_WRITEONLY||status==SDL_IO_STATUS_READONLY){if(!*SDL_GetError())SDL_SetError("Stream read failed");goto done;}
        if(status==SDL_IO_STATUS_EOF){ok=1;break;}
        if(n)continue;
        if(status==SDL_IO_STATUS_NOT_READY){SDL_Delay(1);continue;}
        ok=1;break;
    }
    data[used]=0;
 done:
    if(closeio&&!SDL_CloseIO(src)){ok=0;if(!*SDL_GetError())SDL_SetError("Stream close failed");}
    if(!ok){free(data);return NULL;}if(datasize)*datasize=used;return data;
}
void *SDL_LoadFile(const char *path,size_t *datasize){if(datasize)*datasize=0;SDL_IOStream*s=SDL_IOFromFile(path,"rb");return s?SDL_LoadFile_IO(s,datasize,1):NULL;}

void *SDL_memset(void *p,int c,size_t n){return memset(p,c,n);}
void *SDL_memcpy(void *p,const void *q,size_t n){return memcpy(p,q,n);}
const char *SDL_getenv(const char *name){return getenv(name);}
/* This wasm target does not expose host CPU vector intrinsics. */
/* This virtual target supports SSE2 source intrinsics through wasm_simd128.
   CPU queries describe the guest's available implementation, not host CPUID. */
bool SDL_HasSSE2(void){return 1;}
bool SDL_HasAVX2(void){return 0;}
bool SDL_HasNEON(void){return 0;}
int SDL_abs(int n){return abs(n);}
size_t SDL_GetSIMDAlignment(void){return 16;}
void *SDL_aligned_alloc(size_t a,size_t n){if(a<sizeof(void *))a=sizeof(void *);if((a&(a-1))||n>(size_t)-1-a-sizeof(void *)){SDL_SetError("Invalid aligned allocation");return NULL;}void *base=malloc(n+a+sizeof(void *));if(!base){SDL_SetError("Out of memory");return NULL;}void *p=(void *)(((uintptr_t)base+sizeof(void *)+a-1)&~(a-1));((void **)p)[-1]=base;return p;}
void SDL_aligned_free(void *p){if(p)free(((void **)p)[-1]);}
int SDL_memcmp(const void *a,const void *b,size_t n){return memcmp(a,b,n);}
void *SDL_memset4(void *p,Uint32 v,size_t n){Uint32 *d=p;for(size_t i=0;i<n;i++)d[i]=v;return p;}
void SDL_qsort(void *p,size_t n,size_t s,int (*cmp)(const void *,const void *)){qsort(p,n,s,cmp);}
float SDL_roundf(float v){return roundf(v);}
int SDL_strcmp(const char *a,const char *b){return strcmp(a,b);}
char *SDL_strdup(const char *s){return strdup(s);}
size_t SDL_strlen(const char *s){return strlen(s);}
char *SDL_strrchr(const char *s,int c){return strrchr(s,c);}
size_t SDL_strlcpy(char *dst,const char *src,size_t n){return strlcpy(dst,src,n);}
int SDL_tolower(int c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
int SDL_isdigit(int c){return c>='0'&&c<='9';}
Sint64 SDL_strtoll(const char *s,char **end,int base){return strtoll(s,end,base);}
double SDL_atof(const char *s){return atof(s);}
int SDL_asprintf(char **out,const char *format,...){
    if(!out||!format){SDL_InvalidParamError(!out?"out":"format");return -1;}*out=NULL;
    va_list ap;va_start(ap,format);va_list copy;va_copy(copy,ap);int n=vsnprintf(NULL,0,format,copy);va_end(copy);
    if(n<0){va_end(ap);SDL_SetError("Formatting failed");return -1;}
    char *p=malloc((size_t)n+1);if(!p){va_end(ap);SDL_SetError("Out of memory");return -1;}
    int result=vsnprintf(p,(size_t)n+1,format,ap);va_end(ap);if(result<0){free(p);SDL_SetError("Formatting failed");return -1;}*out=p;return result;
}
float SDL_acosf(float v){return acosf(v);}
double SDL_exp(double v){return exp(v);}
double SDL_log(double v){return log(v);}
double SDL_pow(double a,double b){return pow(a,b);}
double SDL_cos(double v){return cos(v);}
double SDL_sin(double v){return sin(v);}
/* Bounded Unicode conversion for metadata; no locale-dependent host codecs.
   Invalid input and unsupported encodings fail explicitly, never claim success
   after byte copying data in a different encoding. Output always has four zero
   padding bytes, as SDL_iconv_string promises for UTF-8/16/32 callers. */
enum TextEncoding {TEXT_BAD,TEXT_UTF8,TEXT_LATIN1,TEXT_ASCII,TEXT_UCS2BE,TEXT_UCS2LE,TEXT_UTF16BE,TEXT_UTF16LE,TEXT_UTF16,TEXT_UTF32BE,TEXT_UTF32LE,TEXT_UTF32};
static int text_encoding(const char *name){
    char key[32];size_t n=0;if(!name||!*name)return TEXT_UTF8;
    for(;*name;name++){char c=*name;if(c=='-'||c=='_'||c==' ')continue;if(n==sizeof(key)-1)return TEXT_BAD;key[n++]=(char)SDL_tolower((unsigned char)c);}key[n]=0;
    if(!strcmp(key,"utf8"))return TEXT_UTF8;
    if(!strcmp(key,"iso88591")||!strcmp(key,"latin1"))return TEXT_LATIN1;
    if(!strcmp(key,"ascii")||!strcmp(key,"usascii"))return TEXT_ASCII;
    if(!strcmp(key,"ucs2")||!strcmp(key,"ucs2be"))return TEXT_UCS2BE;
    if(!strcmp(key,"ucs2le")||!strcmp(key,"ucs2internal"))return TEXT_UCS2LE;
    if(!strcmp(key,"utf16be"))return TEXT_UTF16BE;
    if(!strcmp(key,"utf16le"))return TEXT_UTF16LE;
    if(!strcmp(key,"utf16"))return TEXT_UTF16;
    if(!strcmp(key,"utf32be")||!strcmp(key,"ucs4be")||!strcmp(key,"ucs4"))return TEXT_UTF32BE;
    if(!strcmp(key,"utf32le")||!strcmp(key,"ucs4le")||!strcmp(key,"ucs4internal"))return TEXT_UTF32LE;
    if(!strcmp(key,"utf32"))return TEXT_UTF32;
    return TEXT_BAD;
}
static Uint32 text_u16(const Uint8*p,bool le){return le?((Uint32)p[1]<<8)|p[0]:((Uint32)p[0]<<8)|p[1];}
static void text_put16(Uint8*p,Uint32 c,bool le){p[le?0:1]=(Uint8)c;p[le?1:0]=(Uint8)(c>>8);}
char *SDL_iconv_string(const char *to,const char *from,const char *input,size_t length){
    int source=text_encoding(from),target=text_encoding(to);
    if(!source||!target){SDL_SetError("Unsupported text encoding");return NULL;}
    if(!input&&length){SDL_InvalidParamError("input");return NULL;}
    if(length>((size_t)-8)/4){SDL_SetError("Text conversion size overflow");return NULL;}
    const Uint8 *p=(const Uint8*)input;Uint8 *out=calloc(length*4+8,1);if(!out){SDL_SetError("Out of memory");return NULL;}Uint8 *w=out;
    if(source==TEXT_UTF16){source=TEXT_UTF16BE;if(length>=2&&p[0]==255&&p[1]==254){source=TEXT_UTF16LE;p+=2;length-=2;}else if(length>=2&&p[0]==254&&p[1]==255){p+=2;length-=2;}}
    if(source==TEXT_UTF32){source=TEXT_UTF32BE;if(length>=4&&p[0]==255&&p[1]==254&&!p[2]&&!p[3]){source=TEXT_UTF32LE;p+=4;length-=4;}else if(length>=4&&!p[0]&&!p[1]&&p[2]==254&&p[3]==255){p+=4;length-=4;}}
    if(target==TEXT_UTF16){target=TEXT_UTF16BE;*w++=254;*w++=255;}
    if(target==TEXT_UTF32){target=TEXT_UTF32BE;*w++=0;*w++=0;*w++=254;*w++=255;}
    while(length){
        Uint32 cp;size_t n;
        if(source==TEXT_LATIN1||source==TEXT_ASCII){cp=p[0];n=1;if(source==TEXT_ASCII&&cp>127)goto malformed;}
        else if(source==TEXT_UTF8){
            Uint8 first=p[0];if(first<128){cp=first;n=1;}else if(first>=194&&first<=223){cp=first&31;n=2;}else if(first>=224&&first<=239){cp=first&15;n=3;}else if(first>=240&&first<=244){cp=first&7;n=4;}else goto malformed;
            if(length<n)goto malformed;
            for(size_t i=1;i<n;i++){if((p[i]&192)!=128)goto malformed;cp=(cp<<6)|(p[i]&63);}
            if((n==2&&cp<128)||(n==3&&cp<2048)||(n==4&&cp<65536))goto malformed;
        }else if(source==TEXT_UTF32BE||source==TEXT_UTF32LE){
            if(length<4)goto malformed;bool le=source==TEXT_UTF32LE;cp=0;for(int i=0;i<4;i++)cp=(cp<<8)|p[le?3-i:i];n=4;
        }else{
            if(length<2)goto malformed;bool le=source==TEXT_UCS2LE||source==TEXT_UTF16LE;cp=text_u16(p,le);n=2;
            if((source==TEXT_UTF16BE||source==TEXT_UTF16LE)&&cp>=0xd800&&cp<=0xdbff){if(length<4)goto malformed;Uint32 low=text_u16(p+2,le);if(low<0xdc00||low>0xdfff)goto malformed;cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);n=4;}
        }
        if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))goto malformed;
        p+=n;length-=n;
        if(target==TEXT_UTF8)w=(Uint8*)SDL_UCS4ToUTF8(cp,(char*)w);
        else if(target==TEXT_LATIN1||target==TEXT_ASCII){if(cp>(target==TEXT_LATIN1?255:127))goto unrepresentable;*w++=(Uint8)cp;}
        else if(target==TEXT_UTF32BE||target==TEXT_UTF32LE){bool le=target==TEXT_UTF32LE;for(int i=0;i<4;i++)w[le?i:3-i]=(Uint8)(cp>>(i*8));w+=4;}
        else{bool le=target==TEXT_UTF16LE||target==TEXT_UCS2LE;if(cp>65535){if(target==TEXT_UCS2LE||target==TEXT_UCS2BE)goto unrepresentable;cp-=0x10000;text_put16(w,0xd800+(cp>>10),le);w+=2;cp=0xdc00+(cp&1023);}text_put16(w,cp,le);w+=2;}
    }
    return (char*)out;
 malformed:free(out);SDL_SetError("Invalid or incomplete encoded text");return NULL;
 unrepresentable:free(out);SDL_SetError("Character cannot be represented in target encoding");return NULL;
}


/* Adapted from SDL 3.2.10 SDL_string.c and SDL_murmur3.c: public functions
   retained, SDL internal assertions/memory/endian macros mapped to libc. */
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
char *SDL_UCS4ToUTF8(Uint32 codepoint, char *dst)
{
    if (!dst) {
        return NULL;  // I guess...?
    } else if (codepoint > 0x10FFFF) {  // Outside the range of Unicode codepoints (also, larger than can be encoded in 4 bytes of UTF-8!).
        codepoint = SDL_INVALID_UNICODE_CODEPOINT;
    } else if ((codepoint >= 0xD800) && (codepoint <= 0xDFFF)) {  // UTF-16 surrogate values are illegal in UTF-8.
        codepoint = SDL_INVALID_UNICODE_CODEPOINT;
    }

    Uint8 *p = (Uint8 *)dst;
    if (codepoint <= 0x7F) {
        *p = (Uint8)codepoint;
        ++dst;
    } else if (codepoint <= 0x7FF) {
        p[0] = 0xC0 | (Uint8)((codepoint >> 6) & 0x1F);
        p[1] = 0x80 | (Uint8)(codepoint & 0x3F);
        dst += 2;
    } else if (codepoint <= 0xFFFF) {
        p[0] = 0xE0 | (Uint8)((codepoint >> 12) & 0x0F);
        p[1] = 0x80 | (Uint8)((codepoint >> 6) & 0x3F);
        p[2] = 0x80 | (Uint8)(codepoint & 0x3F);
        dst += 3;
    } else {
        assert(codepoint <= 0x10FFFF);
        p[0] = 0xF0 | (Uint8)((codepoint >> 18) & 0x07);
        p[1] = 0x80 | (Uint8)((codepoint >> 12) & 0x3F);
        p[2] = 0x80 | (Uint8)((codepoint >> 6) & 0x3F);
        p[3] = 0x80 | (Uint8)(codepoint & 0x3F);
        dst += 4;
    }

    return dst;
}


static Uint32 StepUTF8(const char **_str, const size_t slen)
{
    /*
     * From rfc3629, the UTF-8 spec:
     *  https://www.ietf.org/rfc/rfc3629.txt
     *
     *   Char. number range  |        UTF-8 octet sequence
     *      (hexadecimal)    |              (binary)
     *   --------------------+---------------------------------------------
     *   0000 0000-0000 007F | 0xxxxxxx
     *   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
     *   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
     *   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
     */

    const Uint8 *str = (const Uint8 *) *_str;
    const Uint32 octet = (Uint32) (slen ? *str : 0);

    if (octet == 0) {  // null terminator, end of string.
        return 0;  // don't advance `*_str`.
    } else if ((octet & 0x80) == 0) {  // 0xxxxxxx: one byte codepoint.
        (*_str)++;
        return octet;
    } else if (((octet & 0xE0) == 0xC0) && (slen >= 2)) {  // 110xxxxx 10xxxxxx: two byte codepoint.
        const Uint8 str1 = str[1];
        if ((str1 & 0xC0) == 0x80) {  // If trailing bytes aren't 10xxxxxx, sequence is bogus.
            const Uint32 result = ((octet & 0x1F) << 6) | (str1 & 0x3F);
            if (result >= 0x0080) {  // rfc3629 says you can't use overlong sequences for smaller values.
                *_str += 2;
                return result;
            }
        }
    } else if (((octet & 0xF0) == 0xE0) && (slen >= 3)) {  // 1110xxxx 10xxxxxx 10xxxxxx: three byte codepoint.
        const Uint8 str1 = str[1];
        const Uint8 str2 = str[2];
        if (((str1 & 0xC0) == 0x80) && ((str2 & 0xC0) == 0x80)) {  // If trailing bytes aren't 10xxxxxx, sequence is bogus.
            const Uint32 octet2 = ((Uint32) (str1 & 0x3F)) << 6;
            const Uint32 octet3 = ((Uint32) (str2 & 0x3F));
            const Uint32 result = ((octet & 0x0F) << 12) | octet2 | octet3;
            if (result >= 0x800) {  // rfc3629 says you can't use overlong sequences for smaller values.
                if ((result < 0xD800) || (result > 0xDFFF)) {  // UTF-16 surrogate values are illegal in UTF-8.
                    *_str += 3;
                    return result;
                }
            }
        }
    } else if (((octet & 0xF8) == 0xF0) && (slen >= 4)) {  // 11110xxxx 10xxxxxx 10xxxxxx 10xxxxxx: four byte codepoint.
        const Uint8 str1 = str[1];
        const Uint8 str2 = str[2];
        const Uint8 str3 = str[3];
        if (((str1 & 0xC0) == 0x80) && ((str2 & 0xC0) == 0x80) && ((str3 & 0xC0) == 0x80)) {  // If trailing bytes aren't 10xxxxxx, sequence is bogus.
            const Uint32 octet2 = ((Uint32) (str1 & 0x1F)) << 12;
            const Uint32 octet3 = ((Uint32) (str2 & 0x3F)) << 6;
            const Uint32 octet4 = ((Uint32) (str3 & 0x3F));
            const Uint32 result = ((octet & 0x07) << 18) | octet2 | octet3 | octet4;
            if (result >= 0x10000) {  // rfc3629 says you can't use overlong sequences for smaller values.
                *_str += 4;
                return result;
            }
        }
    }

    // bogus byte, skip ahead, return a REPLACEMENT CHARACTER.
    (*_str)++;
    return SDL_INVALID_UNICODE_CODEPOINT;
}

Uint32 SDL_StepUTF8(const char **pstr, size_t *pslen)
{
    if (!pslen) {
        return StepUTF8(pstr, 4);  // 4 == max codepoint size.
    }
    const char *origstr = *pstr;
    const Uint32 result = StepUTF8(pstr, *pslen);
    *pslen -= (size_t) (*pstr - origstr);
    return result;
}

// Public domain murmur3 32-bit hash algorithm
//
// Adapted from: https://en.wikipedia.org/wiki/MurmurHash

static inline Uint32 murmur_32_scramble(Uint32 k)
{
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

Uint32 SDL_murmur3_32(const void *data, size_t len, Uint32 seed)
{
    const Uint8 *bytes = (const Uint8 *)data;
    Uint32 hash = seed;
    Uint32 k;

    // Read in groups of 4.
    if ((((uintptr_t)bytes) & 3) == 0) {
        // We can do aligned 32-bit reads
        for (size_t i = len >> 2; i--; ) {
            k = *(const Uint32 *)bytes;
            k = (k);
            bytes += sizeof(Uint32);
            hash ^= murmur_32_scramble(k);
            hash = (hash << 13) | (hash >> 19);
            hash = hash * 5 + 0xe6546b64;
        }
    } else {
        for (size_t i = len >> 2; i--; ) {
            SDL_memcpy(&k, bytes, sizeof(Uint32));
            k = (k);
            bytes += sizeof(Uint32);
            hash ^= murmur_32_scramble(k);
            hash = (hash << 13) | (hash >> 19);
            hash = hash * 5 + 0xe6546b64;
        }
    }

    // Read the rest.
    size_t left = (len & 3);
    if (left) {
        k = 0;
        for (size_t i = left; i--; ) {
            k <<= 8;
            k |= bytes[i];
        }

        // A swap is *not* necessary here because the preceding loop already
        // places the low bytes in the low places according to whatever endianness
        // we use. Swaps only apply when the memory is copied in a chunk.
        hash ^= murmur_32_scramble(k);
    }

    /* Finalize. */
    hash ^= len;
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}
