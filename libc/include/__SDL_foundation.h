
#pragma once
__require_source("__SDL_foundation.c");
typedef Uint32 SDL_PropertiesID;
typedef struct SDL_Mutex SDL_Mutex;
typedef struct SDL_RWLock SDL_RWLock;
typedef struct SDL_AtomicInt { int value; } SDL_AtomicInt;
typedef Uint64 SDL_ThreadID;
typedef enum SDL_InitStatus { SDL_INIT_STATUS_UNINITIALIZED, SDL_INIT_STATUS_INITIALIZING, SDL_INIT_STATUS_INITIALIZED, SDL_INIT_STATUS_UNINITIALIZING } SDL_InitStatus;
typedef struct SDL_InitState { SDL_AtomicInt status; SDL_ThreadID thread; void *reserved; } SDL_InitState;
SDL_Mutex *SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_Mutex *m);
void SDL_LockMutex(SDL_Mutex *m);
bool SDL_TryLockMutex(SDL_Mutex *m);
void SDL_UnlockMutex(SDL_Mutex *m);
SDL_RWLock *SDL_CreateRWLock(void);
void SDL_DestroyRWLock(SDL_RWLock *m);
void SDL_LockRWLockForReading(SDL_RWLock *m);
void SDL_LockRWLockForWriting(SDL_RWLock *m);
void SDL_UnlockRWLock(SDL_RWLock *m);
int SDL_AddAtomicInt(SDL_AtomicInt *a,int v);
int SDL_GetAtomicInt(SDL_AtomicInt *a);
int SDL_SetAtomicInt(SDL_AtomicInt *a,int v);
bool SDL_CompareAndSwapAtomicInt(SDL_AtomicInt *a,int old,int val);
bool SDL_ShouldInit(SDL_InitState *s);
bool SDL_ShouldQuit(SDL_InitState *s);
void SDL_SetInitialized(SDL_InitState *s,bool initialized);
SDL_PropertiesID SDL_CreateProperties(void);
void SDL_DestroyProperties(SDL_PropertiesID p);
bool SDL_LockProperties(SDL_PropertiesID p);
void SDL_UnlockProperties(SDL_PropertiesID p);
bool SDL_ClearProperty(SDL_PropertiesID p,const char *name);
bool SDL_CopyProperties(SDL_PropertiesID src,SDL_PropertiesID dst);
typedef void (*SDL_EnumeratePropertiesCallback)(void *userdata,SDL_PropertiesID props,const char *name);
bool SDL_EnumerateProperties(SDL_PropertiesID props,SDL_EnumeratePropertiesCallback callback,void *userdata);
typedef enum SDL_PropertyType { SDL_PROPERTY_TYPE_INVALID,SDL_PROPERTY_TYPE_POINTER,SDL_PROPERTY_TYPE_STRING,SDL_PROPERTY_TYPE_NUMBER,SDL_PROPERTY_TYPE_FLOAT,SDL_PROPERTY_TYPE_BOOLEAN } SDL_PropertyType;
SDL_PropertyType SDL_GetPropertyType(SDL_PropertiesID p,const char *name);
bool SDL_HasProperty(SDL_PropertiesID p,const char *name);
typedef void (*SDL_CleanupPropertyCallback)(void *,void *);
bool SDL_SetPointerPropertyWithCleanup(SDL_PropertiesID p,const char *name,void *value,SDL_CleanupPropertyCallback cleanup,void *userdata);
bool SDL_SetPointerProperty(SDL_PropertiesID p,const char *name,void *value);
void *SDL_GetPointerProperty(SDL_PropertiesID p,const char *name,void *fallback);
bool SDL_SetStringProperty(SDL_PropertiesID p,const char *name,const char *value);
const char *SDL_GetStringProperty(SDL_PropertiesID p,const char *name,const char *fallback);
bool SDL_SetNumberProperty(SDL_PropertiesID p,const char *name,Sint64 value);
Sint64 SDL_GetNumberProperty(SDL_PropertiesID p,const char *name,Sint64 fallback);
bool SDL_SetFloatProperty(SDL_PropertiesID p,const char *name,float value);
float SDL_GetFloatProperty(SDL_PropertiesID p,const char *name,float fallback);
bool SDL_SetBooleanProperty(SDL_PropertiesID p,const char *name,bool value);
bool SDL_GetBooleanProperty(SDL_PropertiesID p,const char *name,bool fallback);
typedef struct SDL_IOStream SDL_IOStream;
typedef enum SDL_IOStatus { SDL_IO_STATUS_READY,SDL_IO_STATUS_ERROR,SDL_IO_STATUS_EOF,SDL_IO_STATUS_NOT_READY,SDL_IO_STATUS_READONLY,SDL_IO_STATUS_WRITEONLY } SDL_IOStatus;
typedef enum SDL_IOWhence { SDL_IO_SEEK_SET,SDL_IO_SEEK_CUR,SDL_IO_SEEK_END } SDL_IOWhence;
typedef struct SDL_IOStreamInterface { Uint32 version; Sint64 (*size)(void *); Sint64 (*seek)(void *,Sint64,SDL_IOWhence); size_t (*read)(void *,void *,size_t,SDL_IOStatus *); size_t (*write)(void *,const void *,size_t,SDL_IOStatus *); bool (*flush)(void *,SDL_IOStatus *); bool (*close)(void *); } SDL_IOStreamInterface;
#define SDL_INIT_INTERFACE(i) do { SDL_memset((i),0,sizeof(*(i))); (i)->version=sizeof(*(i)); } while(0)
#define SDL_PROP_IOSTREAM_STDIO_FILE_POINTER "SDL.iostream.stdio.file"
#define SDL_PROP_IOSTREAM_MEMORY_POINTER "SDL.iostream.memory.base"
#define SDL_PROP_IOSTREAM_MEMORY_SIZE_NUMBER "SDL.iostream.memory.size"
SDL_IOStream *SDL_OpenIO(const SDL_IOStreamInterface *iface,void *userdata);
SDL_IOStream *SDL_IOFromFile(const char *path,const char *mode);
SDL_IOStream *SDL_IOFromMem(void *mem,size_t size);
SDL_IOStream *SDL_IOFromConstMem(const void *mem,size_t size);
SDL_IOStream *SDL_IOFromDynamicMem(void);
#define SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER "SDL.iostream.dynamic.memory"
#define SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER "SDL.iostream.dynamic.chunksize"
void *SDL_LoadFile_IO(SDL_IOStream *src,size_t *datasize,bool closeio);
void *SDL_LoadFile(const char *path,size_t *datasize);
bool SDL_CloseIO(SDL_IOStream *s);
size_t SDL_ReadIO(SDL_IOStream *s,void *ptr,size_t size);
size_t SDL_WriteIO(SDL_IOStream *s,const void *ptr,size_t size);
bool SDL_FlushIO(SDL_IOStream *s);
Sint64 SDL_SeekIO(SDL_IOStream *s,Sint64 offset,SDL_IOWhence whence);
Sint64 SDL_TellIO(SDL_IOStream *s);
Sint64 SDL_GetIOSize(SDL_IOStream *s);
SDL_PropertiesID SDL_GetIOProperties(SDL_IOStream *s);
SDL_IOStatus SDL_GetIOStatus(SDL_IOStream *s);
bool SDL_ReadU8(SDL_IOStream *src,Uint8 *value);
bool SDL_ReadU16LE(SDL_IOStream *src,Uint16 *value);
bool SDL_ReadU16BE(SDL_IOStream *src,Uint16 *value);
bool SDL_ReadU32LE(SDL_IOStream *src,Uint32 *value);
bool SDL_ReadU32BE(SDL_IOStream *src,Uint32 *value);
bool SDL_ReadU64LE(SDL_IOStream *src,Uint64 *value);
bool SDL_ReadU64BE(SDL_IOStream *src,Uint64 *value);
bool SDL_ReadS8(SDL_IOStream *src,Sint8 *value);
bool SDL_ReadS16LE(SDL_IOStream *src,Sint16 *value);
bool SDL_ReadS16BE(SDL_IOStream *src,Sint16 *value);
bool SDL_ReadS32LE(SDL_IOStream *src,Sint32 *value);
bool SDL_ReadS32BE(SDL_IOStream *src,Sint32 *value);
bool SDL_ReadS64LE(SDL_IOStream *src,Sint64 *value);
bool SDL_ReadS64BE(SDL_IOStream *src,Sint64 *value);

void *SDL_memset(void *p,int c,size_t n);
void *SDL_memcpy(void *p,const void *q,size_t n);
int SDL_abs(int n);
void *SDL_aligned_alloc(size_t alignment,size_t size);
size_t SDL_GetSIMDAlignment(void);
void SDL_aligned_free(void *ptr);
int SDL_memcmp(const void *a,const void *b,size_t n);
void *SDL_memset4(void *dst,Uint32 val,size_t dwords);
void SDL_qsort(void *base,size_t count,size_t size,int (*cmp)(const void *,const void *));
float SDL_roundf(float v);
int SDL_strcmp(const char *a,const char *b);
char *SDL_strdup(const char *s);
size_t SDL_strlen(const char *s);
char *SDL_strrchr(const char *s,int c);
size_t SDL_strlcpy(char *dst,const char *src,size_t size);
int SDL_tolower(int c);
int SDL_isdigit(int c);
Sint64 SDL_strtoll(const char *s,char **end,int base);
double SDL_atof(const char *s);
int SDL_asprintf(char **out,const char *format,...);
float SDL_acosf(float value);
double SDL_exp(double value);
double SDL_log(double value);
double SDL_pow(double a,double b);
double SDL_cos(double value);
double SDL_sin(double value);
char *SDL_iconv_string(const char *tocode,const char *fromcode,const char *inbuf,size_t inbytesleft);
Uint32 SDL_StepUTF8(const char **p,size_t *len);
char *SDL_UCS4ToUTF8(Uint32 c,char *dst);
Uint32 SDL_murmur3_32(const void *p,size_t n,Uint32 seed);
typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct SDL_GPUTexture SDL_GPUTexture;
typedef struct SDL_GPUCommandBuffer SDL_GPUCommandBuffer;
typedef struct SDL_GPURenderPass SDL_GPURenderPass;
static inline int SDL_MostSignificantBitIndex32(Uint32 x){int i=-1;while(x){x>>=1;i++;}return i;}
static inline bool SDL_HasExactlyOneBitSet32(Uint32 x){return x&&!(x&(x-1));}
#define SDL_AtomicIncRef(a) SDL_AddAtomicInt((a),1)
#define SDL_AtomicDecRef(a) (SDL_AddAtomicInt((a),-1)==1)
#define SDL_copyp(a,b) SDL_memcpy((a),(b),sizeof(*(a)))
static inline bool SDL_size_add_check_overflow(size_t a,size_t b,size_t *r){if(a>(size_t)-1-b)return 0;*r=a+b;return 1;}
static inline bool SDL_size_mul_check_overflow(size_t a,size_t b,size_t *r){if(b&&a>(size_t)-1/b)return 0;*r=a*b;return 1;}
#define SDL_INVALID_UNICODE_CODEPOINT 0xFFFD
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#define SDL_DECLSPEC
#define SDLCALL
#define SDL_INLINE inline
#define SDL_FORCE_INLINE static inline
#define SDL_PRINTF_FORMAT_STRING
#define SDL_PRINTF_VARARG_FUNC(a,b)
#define SDL_SCANF_FORMAT_STRING
#define SDL_SCANF_VARARG_FUNC(a,b)
#define SDL_MALLOC
#define SDL_ALLOC_SIZE(a)
#define SDL_ALLOC_SIZE2(a,b)
#define SDL_OUT_Z_CAP(a)
#define SDL_IN_BYTECAP(a)
#define SDL_OUT_BYTECAP(a)
#define SDL_NODISCARD
#define SDL_DEPRECATED
#define SDL_MAJOR_VERSION 3
#define SDL_MINOR_VERSION 2
#define SDL_MICRO_VERSION 10
#define SDL_VERSIONNUM(a,b,c) ((a)*1000000+(b)*1000+(c))
#define SDL_VERSION SDL_VERSIONNUM(3,2,10)
#define SDL_VERSION_ATLEAST(a,b,c) (SDL_VERSION >= SDL_VERSIONNUM(a,b,c))
#define SDL_arraysize(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_min(a,b) (((a)<(b))?(a):(b))
#define SDL_max(a,b) (((a)>(b))?(a):(b))
#define SDL_clamp(v,a,b) SDL_min(SDL_max(v,a),b)
#define SDL_zero(a) SDL_memset(&(a),0,sizeof(a))
#define SDL_zerop(a) SDL_memset((a),0,sizeof(*(a)))
#define SDL_zeroa(a) SDL_memset((a),0,sizeof(a))
#define SDL_assert(expr) ((expr)?(void)0:__assert_fail(#expr,__FILE__,__LINE__))
#define SDL_assert_release(expr) SDL_assert(expr)
#define SDL_COMPILE_TIME_ASSERT(name,x) typedef int SDL_compile_assert_##name[(x)?1:-1]
#define SDL_SIZE_MAX 0xffffffffu
#define SDL_MIN_SINT16 (-32768)
#define SDL_MAX_SINT16 32767
#define SDL_MIN_SINT32 (-2147483647-1)
#define SDL_MAX_SINT32 2147483647
#define SDL_MAX_UINT32 0xffffffffu
#define SDL_MAX_SINT64 9223372036854775807LL
#define SDL_MAX_UINT64 18446744073709551615ULL
#define SDL_SIZEOF_VOIDP 4
#define SDL_BYTEORDER 1234
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_Swap16LE(x) (x)
#define SDL_Swap32LE(x) (x)
#define SDL_Swap64LE(x) (x)
#define SDL_memmove memmove
#define SDL_snprintf snprintf
#define SDL_strncmp strncmp
#define SDL_strchr strchr
#define SDL_strstr strstr
const char *SDL_getenv(const char *name);
bool SDL_HasSSE2(void);
bool SDL_HasAVX2(void);
bool SDL_HasNEON(void);
#define SDL_atoi atoi
#define SDL_strtol strtol
#define SDL_sqrt sqrt
#define SDL_sqrtf sqrtf
#define SDL_floor floor
#define SDL_floorf floorf
#define SDL_ceil ceil
#define SDL_ceilf ceilf
#define SDL_fabs fabs
#define SDL_fabsf fabsf
#define SDL_isnan isnan
#define SDL_PI_D 3.14159265358979323846
#define SDL_PI_F 3.14159265358979323846f
#define SDL_PRIu32 "u"
#define SDL_PRId32 "d"
#define SDL_PRIx32 "x"
#define SDL_PRIu64 "llu"
#define SDL_PRId64 "lld"
#define SDL_PRIx64 "llx"
#define SDL_FALLTHROUGH
#ifdef SDL_ENABLE_OLD_NAMES
typedef SDL_Mutex SDL_mutex;
typedef bool SDL_bool;
#define SDL_TRUE 1
#define SDL_FALSE 0
#endif

