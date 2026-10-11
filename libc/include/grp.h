
#pragma once
#include <sys/types.h>
#include <stddef.h>
/* Single-root group database, mirroring <pwd.h>'s single-root passwd: the only
   group is "root" (gid 0); everything else is NULL. */
struct group {
  char  *gr_name;
  char  *gr_passwd;
  gid_t  gr_gid;
  char **gr_mem;
};
static inline struct group *getgrgid(gid_t gid) {
  static char *mem[] = { 0 };
  static struct group root = { "root", "x", 0, mem };
  return gid == 0 ? &root : NULL;
}
static inline struct group *getgrnam(const char *name) {
  static char *mem[] = { 0 };
  static struct group root = { "root", "x", 0, mem };
  if (name == 0) return NULL;
  const char *r = "root";
  while (*name && *name == *r) { name++; r++; }
  return (*name == 0 && *r == 0) ? &root : NULL;
}
/* Enumeration walks the same single entry (gawk's grcat helper). */
static int __gr_cursor;
static inline void setgrent(void) { __gr_cursor = 0; }
static inline void endgrent(void) { __gr_cursor = 0; }
static inline struct group *getgrent(void) {
  if (__gr_cursor) return NULL;
  __gr_cursor = 1;
  return getgrgid(0);
}
  