
#pragma once
/* POSIX basename()/dirname(). Both may modify the input buffer and return a
   pointer into it (or a pointer to a static string for the "." / "/" cases).
   Like glibc's <libgen.h> variants, the static-string returns are shared, so
   these are not thread-safe and the result of one call may be clobbered by the
   next on the "." / "/" paths. */
static inline char *__libgen_last_slash(char *s) {
  char *last = 0;
  for (char *p = s; *p; p++) if (*p == '/') last = p;
  return last;
}
static inline char *basename(char *path) {
  static char dot[] = ".";
  static char root[] = "/";
  if (path == 0 || path[0] == 0) return dot;
  char *end = path;
  while (*end) end++;
  while (end > path && end[-1] == '/') *--end = 0;   /* strip trailing slashes */
  if (end == path) return root;                       /* path was all slashes */
  char *slash = __libgen_last_slash(path);
  return slash ? slash + 1 : path;
}
static inline char *dirname(char *path) {
  static char dot[] = ".";
  static char root[] = "/";
  if (path == 0 || path[0] == 0) return dot;
  char *end = path;
  while (*end) end++;
  while (end > path && end[-1] == '/') *--end = 0;   /* strip trailing slashes */
  if (end == path) return root;                       /* path was all slashes */
  char *slash = __libgen_last_slash(path);
  if (slash == 0) return dot;                         /* no directory component */
  while (slash > path && slash[-1] == '/') slash--;   /* strip dir's trailing slashes */
  if (slash == path) return root;                     /* directory is the root */
  *slash = 0;
  return path;
}
  