
#pragma once
#include <sys/types.h>
#include <stddef.h>

/* Single-root-user password database. getpwuid/getpwnam return a static
   "root" entry (uid/gid 0, home /root, shell /bin/sh); anything else is NULL. */
struct passwd {
  char  *pw_name;
  char  *pw_passwd;
  uid_t  pw_uid;
  gid_t  pw_gid;
  char  *pw_gecos;
  char  *pw_dir;
  char  *pw_shell;
};

static inline struct passwd *getpwuid(uid_t uid) {
  static struct passwd root = { "root", "x", 0, 0, "root", "/root", "/bin/sh" };
  return uid == 0 ? &root : NULL;
}

static inline struct passwd *getpwnam(const char *name) {
  static struct passwd root = { "root", "x", 0, 0, "root", "/root", "/bin/sh" };
  if (!name) return NULL;
  /* tiny strcmp(name, "root") — avoids a <string.h> dependency in this header */
  const char *r = "root";
  while (*name && *name == *r) { name++; r++; }
  return (*name == 0 && *r == 0) ? &root : NULL;
}

/* Reentrant variants (used by e.g. glob's ~ expansion). On a match the
   caller's struct + buffer are filled with the root entry and *result points
   at pwd; on no match *result is NULL and the return is 0; ERANGE if buf is
   too small. */
static inline int __pwd_fill_root(struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
  const char *fields[5] = { "root", "x", "root", "/root", "/bin/sh" };
  size_t need = 0;
  for (int i = 0; i < 5; i++) { const char *s = fields[i]; while (*s) { s++; need++; } need++; }
  if (buflen < need) { *result = (struct passwd *)0; return 34; /* ERANGE */ }
  char *p = buf; char *dst[5];
  for (int i = 0; i < 5; i++) { const char *s = fields[i]; dst[i] = p; while (*s) { *p++ = *s++; } *p++ = 0; }
  pwd->pw_name = dst[0]; pwd->pw_passwd = dst[1];
  pwd->pw_uid = 0; pwd->pw_gid = 0;
  pwd->pw_gecos = dst[2]; pwd->pw_dir = dst[3]; pwd->pw_shell = dst[4];
  *result = pwd;
  return 0;
}
/* Enumeration walks the same single entry (gawk's pwcat helper). */
static int __pw_cursor;
static inline void setpwent(void) { __pw_cursor = 0; }
static inline void endpwent(void) { __pw_cursor = 0; }
static inline struct passwd *getpwent(void) {
  if (__pw_cursor) return NULL;
  __pw_cursor = 1;
  return getpwuid(0);
}
static inline int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
  if (!name) { *result = (struct passwd *)0; return 0; }
  const char *n = name; const char *r = "root";
  while (*n && *n == *r) { n++; r++; }
  if (*n != 0 || *r != 0) { *result = (struct passwd *)0; return 0; }
  return __pwd_fill_root(pwd, buf, buflen, result);
}
static inline int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
  if (uid != 0) { *result = (struct passwd *)0; return 0; }
  return __pwd_fill_root(pwd, buf, buflen, result);
}
  