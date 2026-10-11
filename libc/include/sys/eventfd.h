#pragma once
#include <stdint.h>
#include <unistd.h>
#define EFD_SEMAPHORE 1
#define EFD_NONBLOCK 0x800
#define EFD_CLOEXEC 0x80000
#ifdef __EVENTFD_IMPLEMENTATION
#define __EVENTFD_INLINE
#else
#define __EVENTFD_INLINE static inline
#endif
typedef uint64_t eventfd_t;
__import int eventfd(unsigned int initval, int flags);
__EVENTFD_INLINE int eventfd_read(int fd,eventfd_t *value){return read(fd,value,8)==8?0:-1;}
__EVENTFD_INLINE int eventfd_write(int fd,eventfd_t value){return write(fd,&value,8)==8?0:-1;}

#undef __EVENTFD_INLINE
