
#pragma once
#include <sys/time.h>

/* THE FD LIMIT IS FD_SETSIZE (64) AND IT IS ENFORCED, NOT MERELY DOCUMENTED
   (#637). fds_bits holds FD_SETSIZE bits; an fd outside [0, FD_SETSIZE) would
   index it out of bounds. The FD_* macros below bounds-check every fd against
   [0, FD_SETSIZE) — with (unsigned) folding a negative fd into the same
   rejection — so FD_SET/FD_CLR become a no-op and FD_ISSET reads 0 for an
   out-of-range fd instead of touching stack memory past the struct. This makes
   every direct select() caller memory-safe. Native poll() uses a separate
   descriptor-array transport and has no FD_SETSIZE restriction. */
#define FD_SETSIZE 64

typedef struct {
  unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define __FD_INRANGE(fd) ((unsigned long)(fd) < (unsigned long)FD_SETSIZE)
#define FD_ZERO(set)  do { for (int _i = 0; _i < (int)(sizeof((set)->fds_bits)/sizeof((set)->fds_bits[0])); _i++) (set)->fds_bits[_i] = 0; } while(0)
#define FD_SET(fd, set)   (__FD_INRANGE(fd) ? ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] |= (1UL << ((fd) % (8 * sizeof(unsigned long))))) : 0UL)
#define FD_CLR(fd, set)   (__FD_INRANGE(fd) ? ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] &= ~(1UL << ((fd) % (8 * sizeof(unsigned long))))) : 0UL)
#define FD_ISSET(fd, set) (__FD_INRANGE(fd) ? ((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] & (1UL << ((fd) % (8 * sizeof(unsigned long))))) : 0UL)

__import int __select_impl(int nfds, int *readfds, int *writefds, int *exceptfds, long timeout_sec, long timeout_usec, int has_timeout);

static inline int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
  return __select_impl(nfds,
    readfds ? (int *)readfds->fds_bits : (int *)0,
    writefds ? (int *)writefds->fds_bits : (int *)0,
    exceptfds ? (int *)exceptfds->fds_bits : (int *)0,
    timeout ? timeout->tv_sec : 0,
    timeout ? timeout->tv_usec : 0,
    timeout ? 1 : 0);
}
  