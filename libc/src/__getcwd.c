
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
__import char *__getcwd(char *buf, size_t size);
char *getcwd(char *buf, size_t size) {
  if (buf) {
    if (!size) { errno = EINVAL; return 0; }
    return __getcwd(buf, size);
  }
  /* GNU allocation extension, used by ash when inherited PWD is stale. */
  size_t capacity = size ? size : 128;
  for (;;) {
    char *result = malloc(capacity);
    if (!result) { errno = ENOMEM; return 0; }
    if (__getcwd(result, capacity)) return result;
    int error = errno;
    free(result);
    if (size || error != ERANGE) { errno = error; return 0; }
    if (capacity > (size_t)-1 / 2) { errno = ENOMEM; return 0; }
    capacity *= 2;
  }
}
