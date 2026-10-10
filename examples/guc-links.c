/* Bulk package link publication. gucman retains the publication lock and
 * atomic current-release switch. This replaces per-file shell subprocesses,
 * preserving the rule that unmanaged paths and other packages are untouched. */
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The guest owner performs the existence check and rename in one operation.
 * Native probes use their platform's atomic exclusive-rename interface. */
#ifdef __wasm_posix__
__import int __rename_noreplace(const char *, const char *);
#else
static int __rename_noreplace(const char *from, const char *to) {
#ifdef __APPLE__
  return renameatx_np(AT_FDCWD, from, AT_FDCWD, to, RENAME_EXCL);
#elif defined(__linux__)
  return renameat2(AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE);
#else
  (void)from; (void)to; errno = ENOSYS; return -1;
#endif
}
#endif

static int problem(const char *message, const char *path) {
  fprintf(stderr, "gucman: %s: %s\n", message, path);
  return 1;
}
static int relative_name(const char *name) {
  if (!*name || *name == '/')
    return 0;
  const char *part = name;
  for (;;) {
    const char *end = strchr(part, '/');
    size_t length = end ? (size_t)(end - part) : strlen(part);
    if (!length || (length == 1 && part[0] == '.') ||
        (length == 2 && part[0] == '.' && part[1] == '.'))
      return 0;
    if (!end)
      return 1;
    part = end + 1;
  }
}
static int directory(const char *path, int create) {
  struct stat st;
  if (!lstat(path, &st))
    return S_ISDIR(st.st_mode)
               ? 0
               : problem("invalid installation directory", path);
  if (errno != ENOENT)
    return problem("cannot inspect installation directory", path);
  if (create && mkdir(path, 0755))
    return problem("cannot create installation directory", path);
  return 0;
}
static int parents(char *path, size_t prefix_length, int create) {
  /* The installation prefix already exists. Never follow symlink parents. */
  for (char *p = path + prefix_length; *p; p++)
    if (*p == '/') {
      *p = 0;
      int result = directory(path, create);
      *p = '/';
      if (result)
        return result;
    }
  return 0;
}
static int owned(const char *path, const char *target, int *exists) {
  struct stat st;
  *exists = 0;
  if (lstat(path, &st))
    return errno == ENOENT ? 0
                           : problem("cannot inspect installation path", path);
  *exists = 1;
  if (!S_ISLNK(st.st_mode))
    return problem("refusing to replace unmanaged path", path);
  char previous[PATH_MAX];
  ssize_t n = readlink(path, previous, sizeof previous - 1);
  if (n < 0 || n == (ssize_t)sizeof previous - 1)
    return problem("cannot inspect installation link", path);
  previous[n] = 0;
  return strcmp(previous, target)
             ? problem("file conflict or unmanaged symlink", path)
             : 0;
}
int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "move-noreplace"))
    return __rename_noreplace(argv[2], argv[3]) ?
      problem("cannot move source without replacing destination", argv[3]) : 0;
  if (argc != 5 || (strcmp(argv[1], "check") && strcmp(argv[1], "link")) ||
      argv[2][0] != '/' || argv[3][0] != '/') {
    fputs("usage: guc-links check|link PREFIX PACKAGE_ROOT FILE_LIST\n",
          stderr);
    return 2;
  }
  int create = !strcmp(argv[1], "link"), result = 0;
  FILE *list = fopen(argv[4], "r");
  if (!list)
    return problem("cannot open package file list", argv[4]);
  char name[PATH_MAX];
  while (fgets(name, sizeof name, list)) {
    size_t length = strlen(name);
    if (length && name[length - 1] == '\n')
      name[--length] = 0;
    else if (!feof(list)) {
      result = problem("package path too long", name);
      break;
    }
    if (!relative_name(name)) {
      result = problem("invalid package file name", name);
      break;
    }
    char path[PATH_MAX], target[PATH_MAX];
    int exists;
    if (snprintf(path, sizeof path, "%s/%s", argv[2], name) >=
            (int)sizeof path ||
        snprintf(target, sizeof target, "%s/current/%s", argv[3], name) >=
            (int)sizeof target) {
      result = problem("package path too long", name);
      break;
    }
    if (parents(path, strlen(argv[2]), 0) || owned(path, target, &exists)) {
      result = 1;
      break;
    }
    if (create && !exists &&
        (parents(path, strlen(argv[2]), 1) || symlink(target, path))) {
      result = problem("cannot publish package link", path);
      break;
    }
  }
  if (ferror(list))
    result = problem("cannot read package file list", argv[4]);
  fclose(list);
  return result;
}
