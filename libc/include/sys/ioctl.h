
#pragma once
#include <errno.h>
#include <sys/stat.h>
#define FIONREAD 0x541b

#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCSCTTY 0x540e
__import int __pty_control(int, int);

struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};

__import int __ioctl_tiocgwinsz(int fd, int *rows, int *cols);
__import int __ioctl_tiocswinsz(int fd, int rows, int cols);

static inline int ioctl(int fd, unsigned long request, void *arg) {
  if (request == TIOCSCTTY) return __pty_control(fd,4);
  if (request == FIONREAD) {
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    if (!S_ISFIFO(st.st_mode) && !S_ISSOCK(st.st_mode)) { errno=ENOTTY; return -1; }
    if (!arg) { errno=EFAULT; return -1; }
    *(int *)arg = (int)st.st_size;
    return 0;
  }
  if (request == TIOCGWINSZ) {
    struct winsize *ws = (struct winsize *)arg;
    int rows, cols;
    int r = __ioctl_tiocgwinsz(fd, &rows, &cols);
    if (r == 0) {
      ws->ws_row = (unsigned short)rows;
      ws->ws_col = (unsigned short)cols;
      ws->ws_xpixel = 0;
      ws->ws_ypixel = 0;
    }
    return r;
  }
  if (request == TIOCSWINSZ) {
    /* Pty resize (docs/archive/0020): winsize words + SIGWINCH to the tty's
       foreground pgroup. Needs a kernel; ENOTTY elsewhere. */
    const struct winsize *ws = (const struct winsize *)arg;
    return __ioctl_tiocswinsz(fd, (int)ws->ws_row, (int)ws->ws_col);
  }
  /* Two requests are modelled; anything else is genuinely not implemented for
     this device. Say so through errno — a bare -1 left errno holding whatever
     the previous call put there, which surfaces in a caller that formats it
     (CPython's fcntl.ioctl raises OSError straight off errno) as a random
     unrelated failure rather than "unsupported request". */
  errno = ENOTTY;
  return -1;
}
  