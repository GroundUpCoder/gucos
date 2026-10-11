
#pragma once
__require_source("__errno.c");
/* Per-thread: lives in the calling thread's runtime record. */
int *__errno_location(void);
#define errno (*__errno_location())
#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define ENXIO   6
#define E2BIG   7
#define ENOEXEC 8
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define EDEADLK 35
#define EDEADLOCK EDEADLK
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define EXDEV   18
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOTTY  25
#define EFBIG   27
#define ENOSPC  28
#define ESPIPE  29
#define EROFS   30
#define EPIPE   32
#define EDOM    33
#define ERANGE  34
#define EOVERFLOW 75
#define EILSEQ  84
#define ENAMETOOLONG 36
#define ENOSYS  38
#define ENOTEMPTY 39
/* The BlockFS path walk really raises this: SYMLOOP_MAX is 40 hops and the
   walker sets errno 40 on overrun (host.js _walkHops). It was the one live
   kernel errno with no <errno.h> name, which broke every consumer that spells
   it — CPython's errno.py does "from errno import ELOOP", taking pathlib,
   zipfile, zipapp and compileall down with it (docs/archive/0340). */
#define ELOOP   40
#define EWOULDBLOCK EAGAIN
#define ENOLCK    37
#define ETIMEDOUT 110
#define ENOTSOCK     88
#define EDESTADDRREQ 89
#define EPROTOTYPE   91
#define EMSGSIZE 90
#define ENOPROTOOPT 92
#define EPROTONOSUPPORT 93
#define EOPNOTSUPP   95
#define ENOTSUP      EOPNOTSUPP
#define EAFNOSUPPORT 97
#define EADDRINUSE   98
#define EADDRNOTAVAIL 99
#define ENETUNREACH  101
#define ECONNABORTED 103
#define ECONNRESET   104
#define ENOBUFS      105
#define EISCONN      106
#define ENOTCONN     107
#define ECONNREFUSED 111
#define EHOSTUNREACH 113
#define EALREADY    114
#define EINPROGRESS 115
  