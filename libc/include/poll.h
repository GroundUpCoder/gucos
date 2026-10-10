
#pragma once
#include <sys/select.h>
typedef unsigned long nfds_t;
struct pollfd {
  int   fd;
  short events;
  short revents;
};
#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM POLLOUT
#define POLLWRBAND 0x100
/* Native descriptor-array transport; no fd_set size restriction. */
__import int __poll_impl(struct pollfd *fds, nfds_t nfds, int timeout);
static inline int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  return __poll_impl(fds, nfds, timeout);
}

  