
#pragma once
#include <sys/types.h>
#include <errno.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 20

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t     c_cc[NCCS];
  speed_t  c_ispeed;
  speed_t  c_ospeed;
};

#define IGNBRK  0x00001
#define BRKINT  0x00002
#define IGNPAR  0x00004
#define PARMRK  0x00008
#define INPCK   0x00010
#define ISTRIP  0x00020
#define INLCR   0x00040
#define IGNCR   0x00080
#define ICRNL   0x00100
#define IXON    0x00200
#define IXOFF   0x00400
#define IXANY   0x00800
#define IMAXBEL 0x02000
#define IUTF8   0x04000

#define OPOST   0x00001
#define ONLCR   0x00002
#define OCRNL   0x00004

#define CSIZE   0x00300
#define CS5     0x00000
#define CS6     0x00100
#define CS7     0x00200
#define CS8     0x00300
#define CSTOPB  0x00400
#define CREAD   0x00800
#define PARENB  0x01000
#define PARODD  0x02000
#define HUPCL   0x04000
#define CLOCAL  0x08000

#define ECHOE   0x00002
#define ECHOK   0x00004
#define ECHO    0x00008
#define ECHONL  0x00010
#define ISIG    0x00080
#define ICANON  0x00100
#define ECHOCTL 0x00200
#define IEXTEN  0x00400
#define TOSTOP  0x00800
#define NOFLSH  0x80000000

#define VEOF    0
#define VEOL    1
#define VERASE  3
#define VKILL   5
#define VINTR   8
#define VQUIT   9
#define VSUSP   10
#define VSTART  12
#define VSTOP   13
#define VMIN    16
#define VTIME   17

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflush() queue selectors and tcflow() actions — Linux's numbering. */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* Terminal rates. This libc encodes a speed as the LITERAL baud number (the
   BSD convention) rather than Linux's small enum — that is what the original
   B9600/B115200 pair chose, and cfsetispeed/cfgetispeed round-trip the value
   itself, so the whole ladder follows suit. Linux's CBAUD/CBAUDEX/EXTA/EXTB
   name bitfields of the *enum* encoding and have no meaning under this one,
   so they are deliberately absent rather than given invented values. */
#define B0      0
#define B50     50
#define B75     75
#define B110    110
#define B134    134
#define B150    150
#define B200    200
#define B300    300
#define B600    600
#define B1200   1200
#define B1800   1800
#define B2400   2400
#define B4800   4800
#define B9600   9600
#define B19200  19200
#define B38400  38400
#define B57600  57600
#define B115200 115200
#define B230400 230400
#define B460800 460800
#define B500000 500000
#define B576000 576000
#define B921600 921600
#define B1000000 1000000
#define B1152000 1152000
#define B1500000 1500000
#define B2000000 2000000
#define B2500000 2500000
#define B3000000 3000000
#define B3500000 3500000
#define B4000000 4000000

/* Full-struct transfer (control chars included) — the host reads/writes the
   struct termios layout directly: 4 x u32 flags, c_cc[NCCS]@16, speeds@36/40.
   With kernel.js attached these are RPCs into the kernel's line discipline
   (docs/KERNEL.md Phase 3); without one, host defaults answer (canned
   values / the CLI's real terminal). The pgrp pair is ENOTTY without a
   kernel — no process groups to route to. */
__import int __tty_getattr(int fd, struct termios *t);
__import int __tty_setattr(int fd, int actions, const struct termios *t);
__import int __tty_getpgrp(int fd);
__import int __tty_setpgrp(int fd, int pgid);

static inline int tcgetattr(int fd, struct termios *t) {
  return __tty_getattr(fd, t);
}

static inline int tcsetattr(int fd, int actions, const struct termios *t) {
  return __tty_setattr(fd, actions, t);
}

static inline pid_t tcgetpgrp(int fd) { return (pid_t)__tty_getpgrp(fd); }
static inline int tcsetpgrp(int fd, pid_t pgrp) { return __tty_setpgrp(fd, (int)pgrp); }

static inline void cfmakeraw(struct termios *t) {
  t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  t->c_oflag &= ~OPOST;
  t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t->c_cflag &= ~(CSIZE | PARENB);
  t->c_cflag |= CS8;
  t->c_cc[VMIN] = 1;
  t->c_cc[VTIME] = 0;
}

static inline speed_t cfgetispeed(const struct termios *t) { return t->c_ispeed; }
static inline speed_t cfgetospeed(const struct termios *t) { return t->c_ospeed; }
static inline int cfsetispeed(struct termios *t, speed_t s) { t->c_ispeed = s; return 0; }
static inline int cfsetospeed(struct termios *t, speed_t s) { t->c_ospeed = s; return 0; }

/* The line-control quartet (docs/archive/0325 Group D, landed by docs/archive/0340 for
   CPython's termios module). A gucOS terminal is a kernel object, not a
   serial line: a write reaches the line discipline synchronously so there is
   no output queue to drain, there is no carrier to break, and nothing
   transmits under XON/XOFF — tcdrain, tcsendbreak and tcflow are therefore
   COMPLETE on a valid terminal fd, not stubs standing in for missing work.
   Each still resolves the fd first, so a non-terminal or closed fd reports
   ENOTTY/EBADF exactly as POSIX requires instead of succeeding vacuously.

   tcflush is the one that leaves real semantics on the table: the kernel's
   line discipline DOES hold an input queue, and discarding it needs a kernel
   RPC. No shipping consumer asks for it (nothing in the CPython stdlib
   outside its own test suite calls tcflush), so it validates and reports
   success WITHOUT discarding — recorded in docs/LIABILITIES.md against
   docs/archive/0325 rather than left to be rediscovered. */
static inline int __tty_probe_fd(int fd) {
  struct termios __t;
  return __tty_getattr(fd, &__t);
}
static inline int tcdrain(int fd) { return __tty_probe_fd(fd); }
static inline int tcsendbreak(int fd, int duration) {
  (void)duration;
  return __tty_probe_fd(fd);
}
static inline int tcflow(int fd, int action) {
  if (action != TCOOFF && action != TCOON && action != TCIOFF && action != TCION) {
    errno = EINVAL;
    return -1;
  }
  return __tty_probe_fd(fd);
}
static inline int tcflush(int fd, int queue_selector) {
  if (queue_selector != TCIFLUSH && queue_selector != TCOFLUSH &&
      queue_selector != TCIOFLUSH) {
    errno = EINVAL;
    return -1;
  }
  return __tty_probe_fd(fd);
}
