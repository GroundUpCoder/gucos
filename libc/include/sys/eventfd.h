#pragma once
#include <stdint.h>
#include <unistd.h>
#define EFD_SEMAPHORE 1
#define EFD_NONBLOCK 0x800
#define EFD_CLOEXEC 0x80000
typedef uint64_t eventfd_t;
__import int eventfd(unsigned int initval, int flags);
static inline int eventfd_read(int fd,eventfd_t *value){return read(fd,value,8)==8?0:-1;}
static inline int eventfd_write(int fd,eventfd_t value){return write(fd,&value,8)==8?0:-1;}
