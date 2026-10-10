
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

__import int __opendir(const char *name);
__import int __readdir(int handle, void *dirent_buf);
__import int __closedir(int handle);

struct __DIR {
  int fd;
  struct dirent ent;
  char name[1024]; /* retained for rewinddir (no host rewind import) */
};

DIR *opendir(const char *name) {
  int handle = __opendir(name);
  if (handle < 0) return (DIR *)0;
  DIR *dirp = (DIR *)malloc(sizeof(DIR));
  if (!dirp) { __closedir(handle); return (DIR *)0; }
  dirp->fd = handle;
  dirp->name[0] = 0;
  if (name) {
    size_t n = strlen(name);
    if (n < sizeof(dirp->name)) memcpy(dirp->name, name, n + 1);
  }
  return dirp;
}

int closedir(DIR *dirp) {
  if (!dirp) return -1;
  int ret = __closedir(dirp->fd);
  free(dirp);
  return ret;
}

struct dirent *readdir(DIR *dirp) {
  if (!dirp) return (struct dirent *)0;
  int result = __readdir(dirp->fd, &dirp->ent);
  if (result < 0) return (struct dirent *)0;
  return &dirp->ent;
}

/* POSIX rewinddir: reset the stream to the start. With no host rewind
   import, re-open the directory by the name captured at opendir. */
void rewinddir(DIR *dirp) {
  if (!dirp || !dirp->name[0]) return;
  int handle = __opendir(dirp->name);
  if (handle < 0) return;
  __closedir(dirp->fd);
  dirp->fd = handle;
}
  