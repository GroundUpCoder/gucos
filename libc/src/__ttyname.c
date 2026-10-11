
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
/* Browser terminals and openpty pairs are anonymous kernel endpoints, with
   no corresponding named device in /dev. Do not invent a reopenable path. */
int ttyname_r(int fd, char *buffer, size_t size) {
  int saved = errno;
  int result;
  if (fcntl(fd, F_GETFD) < 0) result = errno;
  else if (!isatty(fd)) result = ENOTTY;
  else result = ENODEV;
  errno = saved;
  return result;
}
char *ttyname(int fd) {
  int error = ttyname_r(fd, NULL, 0);
  errno = error;
  return NULL;
}
