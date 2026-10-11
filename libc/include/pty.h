
#pragma once
/* Ptys over the kernel pty layer (docs/archive/0020). openpty() creates a
   master/slave pair; the slave is a full kernel tty (line discipline,
   termios, job control), the master is the terminal application's end.
   termp/winp are accepted for API familiarity but the caller should
   tcsetattr/TIOCSWINSZ explicitly (name, termp, winp may be NULL). */
#include <termios.h>
#include <sys/ioctl.h>

__import int __openpty(int *amaster, int *aslave);

static inline int openpty(int *amaster, int *aslave, char *name,
                          const struct termios *termp,
                          const struct winsize *winp) {
  if (name) name[0] = '\0';
  int r = __openpty(amaster, aslave);
  if (r != 0) return -1;
  if (termp) tcsetattr(*aslave, TCSANOW, termp);
  if (winp) ioctl(*amaster, TIOCSWINSZ, (void *)winp);
  return 0;
}
  