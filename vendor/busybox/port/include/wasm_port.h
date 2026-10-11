/* Browser compatibility helpers. Process creation uses libc fork/execve;
 * there is no process-operation interception or vfork journal. */
#ifndef BB_WASM_PORT_H
#define BB_WASM_PORT_H 1

#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>   /* strcasecmp lives here, not in string.h */
#include <signal.h>
#include <getopt.h>    /* this libc splits getopt out of unistd.h */
#include <alloca.h>

#include <wchar.h>     /* mbstate_t for unicode.c (glibc leaks it around) */

/* ---- glibc-isms not covered by libbb/platform.c's fallbacks ---- */
static ALWAYS_INLINE char *strndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    char *d;
    if (len > n) len = n;
    d = (char *)malloc(len + 1);
    if (d) { memcpy(d, s, len); d[len] = 0; }
    return d;
}
/* Single-user, single-session system: everything is session 1, uid/gid 0,
 * and "changing" effective ids is a successful no-op (test.c drops privs
 * around access checks; there are none to drop). */
static ALWAYS_INLINE int setegid(gid_t g) { (void)g; return 0; }
static ALWAYS_INLINE int seteuid(uid_t u) { (void)u; return 0; }

static ALWAYS_INLINE int sigisemptyset(const sigset_t *s)
{
    return *s == 0;   /* our sigset_t is a plain bitmask word */
}
#define setpgrp() setpgid(0, 0)
/* umask: was shimmed here as a value that round-tripped but did nothing —
 * "the fs layer has no notion of a process umask". The libc has a REAL one
 * now (docs/archive/0382 gap 1) which open/mkdir actually apply, so the shim is
 * both a duplicate definition and a lie. hush's `umask` builtin now takes
 * effect on files its children create. */


#endif /* BB_WASM_PORT_H */
