
#include <stdlib.h>
#include <stdio.h>
#include <string.h>     // mkstemp() uses strlen
#include <fcntl.h>      // mkstemp() uses open
#include <unistd.h>     // mktemp() probes with access()
#include <sys/stat.h>   // mkdtemp() uses mkdir
#include <errno.h>
#include <inttypes.h>
#include <__atexit.h>
#include <spawn.h>      // system() spawns /bin/sh
#include <sys/wait.h>   // waitpid()
#include <signal.h>     // abort() raises SIGABRT

/* mkstemp: POSIX temp-file creation (sed -i and friends). Replaces the
   trailing XXXXXX and opens O_CREAT|O_EXCL. Uniqueness comes from time +
   pid + a counter, like tmpfile() — no anti-tamper story is needed on a
   single-user fs. */
__import long long __time_now(void);
static int __mkstemp_counter;
int mkstemp(char *template_) {
  size_t len = strlen(template_);
  char *x = template_ + len - 6;
  if (len < 6) { errno = EINVAL; return -1; }
  for (int i = 0; i < 6; i++) {
    if (x[i] != 'X') { errno = EINVAL; return -1; }
  }
  for (int tries = 0; tries < 100; tries++) {
    unsigned long v = (unsigned long)__time_now()
        ^ ((unsigned long)getpid() << 8) ^ (unsigned long)__mkstemp_counter++;
    for (int i = 0; i < 6; i++) { x[i] = (char)('a' + v % 26); v /= 26; }
    int fd = open(template_, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) return fd;
    if (errno != EEXIST) return -1;
  }
  errno = EEXIST;
  return -1;
}

/* mktemp/mkdtemp: same XXXXXX churn as mkstemp (busybox's mktemp applet
   wants all three — docs/archive/0034). mktemp() only NAMES a path (that's why
   POSIX withdrew it — the classic TOCTOU); callers here accept that.
   Failure protocol differs per spec: mktemp returns template_ with
   template_[0] = '\0', mkdtemp returns NULL. */
static int __mktemp_spin(char *template_, int (*probe)(const char *)) {
  size_t len = strlen(template_);
  char *x = template_ + len - 6;
  if (len < 6) { errno = EINVAL; return -1; }
  for (int i = 0; i < 6; i++) {
    if (x[i] != 'X') { errno = EINVAL; return -1; }
  }
  for (int tries = 0; tries < 100; tries++) {
    unsigned long v = (unsigned long)__time_now()
        ^ ((unsigned long)getpid() << 8) ^ (unsigned long)__mkstemp_counter++;
    for (int i = 0; i < 6; i++) { x[i] = (char)('a' + v % 26); v /= 26; }
    if (probe(template_) == 0) return 0;
    if (errno != EEXIST) return -1;
  }
  errno = EEXIST;
  return -1;
}
static int __mktemp_probe_free(const char *path) {
  if (access(path, F_OK) != 0) return 0;  /* name is free — good */
  errno = EEXIST;
  return -1;
}
static int __mktemp_probe_mkdir(const char *path) {
  return mkdir(path, 0700);
}
char *mktemp(char *template_) {
  if (__mktemp_spin(template_, __mktemp_probe_free) != 0) template_[0] = '\0';
  return template_;
}
char *mkdtemp(char *template_) {
  if (__mktemp_spin(template_, __mktemp_probe_mkdir) != 0) return 0;
  return template_;
}

int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }

int atoi(const char *nptr) { return (int)strtol(nptr, (char **)0, 10); }
long atol(const char *nptr) { return strtol(nptr, (char **)0, 10); }

static int __digit_value(char c, int base) {
  int v;
  if (c >= '0' && c <= '9') v = c - '0';
  else if (c >= 'a' && c <= 'z') v = c - 'a' + 10;
  else if (c >= 'A' && c <= 'Z') v = c - 'A' + 10;
  else return -1;
  if (v >= base) return -1;
  return v;
}

// Core integer parser: accumulates magnitude as unsigned long long.
// Returns the parsed magnitude. Sets *neg, *any, *overflow, and *endp.
static unsigned long long __strtou_core(
    const char *nptr, const char **endp, int base,
    int *neg, int *any, int *overflow) {
  const char *s = nptr;
  /* C99 7.20.1.4: base must be 0 or 2..36; otherwise EINVAL, value 0,
     endptr = nptr. */
  if (base != 0 && (base < 2 || base > 36)) {
    errno = EINVAL;
    *endp = nptr;
    *neg = 0; *any = 0; *overflow = 0;
    return 0;
  }
  while (*s == ' ' || *s == '\t' || *s == '\n' ||
         *s == '\r' || *s == '\f' || *s == '\v')
    s++;
  *neg = 0;
  if (*s == '-') { *neg = 1; s++; }
  else if (*s == '+') { s++; }
  if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
      __digit_value(s[2], 16) >= 0) {
    base = 16; s += 2;
  } else if (base == 0 && s[0] == '0') {
    /* Octal — leave the '0' for the digit loop so the any-flag and
       endptr are right for a plain "0" (and "0x" without hex digits). */
    base = 8;
  } else if (base == 0) {
    base = 10;
  }
  unsigned long long result = 0;
  *any = 0;
  *overflow = 0;
  while (1) {
    int d = __digit_value(*s, base);
    if (d < 0) break;
    *any = 1;
    if (result > (18446744073709551615ULL - (unsigned)d) / (unsigned)base) *overflow = 1;
    if (!*overflow) result = result * base + d;
    s++;
  }
  *endp = s;
  return result;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
  const char *end;
  int neg, any, overflow;
  unsigned long long val = __strtou_core(nptr, &end, base, &neg, &any, &overflow);
  if (overflow) { errno = ERANGE; val = 18446744073709551615ULL; }
  else if (neg) { val = -val; }
  if (endptr) *endptr = (char *)(any ? end : nptr);
  return val;
}

long long strtoll(const char *nptr, char **endptr, int base) {
  const char *end;
  int neg, any, overflow;
  unsigned long long val = __strtou_core(nptr, &end, base, &neg, &any, &overflow);
  if (overflow || (!neg && val > 9223372036854775807ULL) ||
      (neg && val > 9223372036854775808ULL)) {
    errno = ERANGE;
    if (endptr) *endptr = (char *)(any ? end : nptr);
    return neg ? (-9223372036854775807LL - 1LL) : 9223372036854775807LL;
  }
  if (endptr) *endptr = (char *)(any ? end : nptr);
  return neg ? -(long long)val : (long long)val;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
  const char *end;
  int neg, any, overflow;
  unsigned long long val = __strtou_core(nptr, &end, base, &neg, &any, &overflow);
  if (overflow || val > 4294967295ULL) { errno = ERANGE; val = 4294967295ULL; neg = 0; }
  if (endptr) *endptr = (char *)(any ? end : nptr);
  return neg ? -(unsigned long)val : (unsigned long)val;
}

long strtol(const char *nptr, char **endptr, int base) {
  const char *end;
  int neg, any, overflow;
  unsigned long long val = __strtou_core(nptr, &end, base, &neg, &any, &overflow);
  if (overflow || (!neg && val > 2147483647ULL) ||
      (neg && val > 2147483648ULL)) {
    errno = ERANGE;
    if (endptr) *endptr = (char *)(any ? end : nptr);
    return neg ? (-2147483647L - 1L) : 2147483647L;
  }
  if (endptr) *endptr = (char *)(any ? end : nptr);
  return neg ? -(long)val : (long)val;
}

double atof(const char *nptr) {
  return strtod(nptr, (char **)0);
}

long long atoll(const char *nptr) {
  return strtoll(nptr, (char **)0, 10);
}

long long llabs(long long n) { return n < 0 ? -n : n; }

intmax_t imaxabs(intmax_t n) { return n < 0 ? -n : n; }

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
  imaxdiv_t r;
  r.quot = numer / denom;
  r.rem = numer % denom;
  return r;
}

intmax_t strtoimax(const char *nptr, char **endptr, int base) {
  return (intmax_t)strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base) {
  return (uintmax_t)strtoull(nptr, endptr, base);
}

static unsigned long __rand_next = 1;
int rand(void) {
  __rand_next = __rand_next * 1103515245 + 12345;
  return (__rand_next / 65536) % 32768;
}
void srand(unsigned int seed) { __rand_next = seed; }

/* ---- random()/srandom()/initstate()/setstate() (POSIX XSI, ticket #112) ----
 *
 * Ported from musl 1.2.5 src/prng/random.c (MIT), locks stripped (this
 * target is single-threaded) and file-scope names prefixed __rnd_ so they
 * cannot collide inside this combined translation unit. The lagged Fibonacci
 * generator and its LCG seeding are byte-identical to musl's, so the default
 * sequence equals srandom(1)'s, as the libc-test random test requires. */

static unsigned int __rnd_init_tab[] = {
0x00000000,0x5851f42d,0xc0b18ccf,0xcbb5f646,
0xc7033129,0x30705b04,0x20fd5db4,0x9a8b7f78,
0x502959d8,0xab894868,0x6c0356a7,0x88cdb7ff,
0xb477d43f,0x70a3a52b,0xa8e4baf1,0xfd8341fc,
0x8ae16fd9,0x742d2f7a,0x0d1f0796,0x76035e09,
0x40f7702c,0x6fa72ca5,0xaaa84157,0x58a0df74,
0xc74a0364,0xae533cc4,0x04185faf,0x6de3b115,
0x0cab8628,0xf043bfa4,0x398150e9,0x37521657};

static int __rnd_n = 31;
static int __rnd_i = 3;
static int __rnd_j = 0;
/* &tab[1], not tab+1: array-plus-int on an array rvalue draws this
   compiler's decay warning into every program that links stdlib. */
static unsigned int *__rnd_x = &__rnd_init_tab[1];

static unsigned int __rnd_lcg31(unsigned int x) {
  return (1103515245u * x + 12345u) & 0x7fffffff;
}

static unsigned long long __rnd_lcg64(unsigned long long x) {
  return 6364136223846793005ull * x + 1;
}

static void *__rnd_savestate(void) {
  __rnd_x[-1] = (__rnd_n << 16) | (__rnd_i << 8) | __rnd_j;
  return __rnd_x - 1;
}

static void __rnd_loadstate(unsigned int *state) {
  __rnd_x = state + 1;
  __rnd_n = __rnd_x[-1] >> 16;
  __rnd_i = (__rnd_x[-1] >> 8) & 0xff;
  __rnd_j = __rnd_x[-1] & 0xff;
}

static void __rnd_srandom(unsigned seed) {
  int k;
  unsigned long long s = seed;

  if (__rnd_n == 0) {
    __rnd_x[0] = s;
    return;
  }
  __rnd_i = __rnd_n == 31 || __rnd_n == 7 ? 3 : 1;
  __rnd_j = 0;
  for (k = 0; k < __rnd_n; k++) {
    s = __rnd_lcg64(s);
    __rnd_x[k] = s >> 32;
  }
  /* make sure x contains at least one odd number */
  __rnd_x[0] |= 1;
}

void srandom(unsigned seed) {
  __rnd_srandom(seed);
}

char *initstate(unsigned seed, char *state, size_t size) {
  void *old;

  if (size < 8)
    return 0;
  old = __rnd_savestate();
  if (size < 32)
    __rnd_n = 0;
  else if (size < 64)
    __rnd_n = 7;
  else if (size < 128)
    __rnd_n = 15;
  else if (size < 256)
    __rnd_n = 31;
  else
    __rnd_n = 63;
  __rnd_x = (unsigned int *)state + 1;
  __rnd_srandom(seed);
  __rnd_savestate();
  return old;
}

char *setstate(char *state) {
  void *old;

  old = __rnd_savestate();
  __rnd_loadstate((unsigned int *)state);
  return old;
}

long random(void) {
  long k;

  if (__rnd_n == 0) {
    k = __rnd_x[0] = __rnd_lcg31(__rnd_x[0]);
    return k;
  }
  __rnd_x[__rnd_i] += __rnd_x[__rnd_j];
  k = __rnd_x[__rnd_i] >> 1;
  if (++__rnd_i == __rnd_n)
    __rnd_i = 0;
  if (++__rnd_j == __rnd_n)
    __rnd_j = 0;
  return k;
}

void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *)) {
  size_t lo = 0;
  size_t hi = nmemb;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    const void *p = (const char *)base + mid * size;
    int cmp = compar(key, p);
    if (cmp < 0) hi = mid;
    else if (cmp > 0) lo = mid + 1;
    else return (void *)p;
  }
  return (void *)0;
}

static void __swap(char *a, char *b, size_t size) {
  size_t i = 0;
  while (i < size) {
    char t = a[i];
    a[i] = b[i];
    b[i] = t;
    i++;
  }
}

static void __siftdown(char *base, size_t nmemb, size_t size, size_t i,
                        int (*compar)(const void *, const void *)) {
  while (1) {
    size_t left = 2 * i + 1;
    size_t right = 2 * i + 2;
    size_t largest = i;
    if (left < nmemb &&
        compar(base + left * size, base + largest * size) > 0)
      largest = left;
    if (right < nmemb &&
        compar(base + right * size, base + largest * size) > 0)
      largest = right;
    if (largest == i) break;
    __swap(base + i * size, base + largest * size, size);
    i = largest;
  }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  if (nmemb < 2) return;
  char *b = (char *)base;
  // Build max-heap
  size_t i = nmemb / 2;
  while (i > 0) {
    i--;
    __siftdown(b, nmemb, size, i, compar);
  }
  // Extract elements
  size_t end = nmemb;
  while (end > 1) {
    end--;
    __swap(b, b + end * size, size);
    __siftdown(b, end, size, 0, compar);
  }
}

__import void __exit(int status);

void exit(int status) {
  /* C11 7.22.4.4: atexit handlers run first (they may still write to
     streams), then streams are flushed and closed. */
  __run_atexits();
  fflush(0);
  __exit(status);
}
__export exit = exit;


__import void __abort_report(void);

void abort(void) {
  /* C11/POSIX: raise SIGABRT. If a handler is installed it runs (synchronous
     self-delivery); abort then terminates regardless (134 = 128+SIGABRT),
     bypassing atexit handlers. */
  /* Capture the live callers before default SIGABRT can reap this worker.
     A separate diagnostic import keeps ordinary exit(134) silent. */
  __abort_report();
  raise(SIGABRT);
  __exit(134);
}

div_t div(int numer, int denom) {
  div_t r;
  r.quot = numer / denom;
  r.rem = numer % denom;
  return r;
}

ldiv_t ldiv(long numer, long denom) {
  ldiv_t r;
  r.quot = numer / denom;
  r.rem = numer % denom;
  return r;
}

lldiv_t lldiv(long long numer, long long denom) {
  lldiv_t r;
  r.quot = numer / denom;
  r.rem = numer % denom;
  return r;
}

/* POSIX environment — environ is the source of truth: getenv() walks it,
   setenv/unsetenv/putenv/clearenv mutate it. Empty by default; the host
   populates it via __set_environ() (see below). The initial block the host
   installs lives in wasm stack memory and is NOT heap-owned, so the first
   mutation deep-copies it to the heap (tracked by __environ_owned).

   Ownership is PER STRING, not per block (#296): putenv() follows POSIX and
   installs the CALLER's pointer verbatim — later edits to that buffer are
   visible through getenv(), and the libc must never free such an entry (the
   caller still owns it; busybox hush re-exports its environ-imported PWD via
   putenv(varstr) where varstr IS the environ entry, and frees a replaced
   exported var's old string ITSELF after putenv(new)). So the libc keeps a
   registry of exactly the strings it allocated into environ (setenv entries
   and the take-ownership deep copies) and frees an entry only when the
   registry says it may. (The public extern char **environ is declared in the
   headers.) */
static char *__environ_empty[] = { 0 };
char **environ = __environ_empty;

/* Forward decls — these string helpers are defined later in this unit. */
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strdup(const char *s);

static int __environ_owned = 0;

/* The ownership registry: strings this libc malloc'd into environ and may
   free later — setenv-allocated entries ONLY. The take-ownership deep copies
   of the inherited environment are deliberately NOT registered: environ
   importers (busybox hush's startup loop) alias inherited strings for the
   process lifetime on the musl/glibc guarantee that an execve'd environment
   is never freed, so freeing one on unsetenv/replacement is a use-after-free
   under the importer (#312). Unsorted; NULL slots are reusable. Environments
   are small, so linear scans suffice. */
static char **__environ_mine = 0;
static int __environ_mine_cap = 0;

static void __environ_mine_add(char *p) {
  for (int i = 0; i < __environ_mine_cap; i++) {
    if (!__environ_mine[i]) { __environ_mine[i] = p; return; }
  }
  int cap = __environ_mine_cap ? __environ_mine_cap * 2 : 8;
  char **m = realloc(__environ_mine, cap * sizeof(char *));
  if (!m) return; /* untracked: the entry leaks later instead of double-freeing */
  for (int i = __environ_mine_cap; i < cap; i++) m[i] = 0;
  m[__environ_mine_cap] = p;
  __environ_mine = m;
  __environ_mine_cap = cap;
}

/* Free p iff this libc allocated it into environ; caller-owned putenv()
   strings pass through untouched. */
static void __environ_dispose(char *p) {
  for (int i = 0; i < __environ_mine_cap; i++) {
    if (__environ_mine[i] == p) {
      __environ_mine[i] = 0;
      free(p);
      return;
    }
  }
}

static int __environ_count(void) {
  int n = 0;
  while (environ[n]) n++;
  return n;
}

static void __environ_take_ownership(void) {
  if (__environ_owned) return;
  int n = __environ_count();
  char **heap = malloc((n + 1) * sizeof(char *));
  /* Immortal copies: NOT registered, so unsetenv/replacement never frees
     them (see the registry comment). Unsetting or replacing an inherited
     entry leaks its copy — bounded by one initial environment per process;
     glibc leaks the same way by design. */
  for (int i = 0; i < n; i++)
    heap[i] = strdup(environ[i]);
  heap[n] = 0;
  environ = heap;
  __environ_owned = 1;
}

/* Allocate a heap "name=value" string. */
static char *__environ_entry(const char *name, const char *value) {
  size_t nlen = strlen(name);
  char *e = malloc(nlen + 1 + strlen(value) + 1);
  if (!e) return 0;
  strcpy(e, name);
  e[nlen] = '=';
  strcpy(e + nlen + 1, value);
  return e;
}

/* Index of the variable named "name" (nlen bytes), or -1 if absent. */
static int __environ_find(const char *name, size_t nlen) {
  for (int i = 0; environ[i]; i++) {
    if (strncmp(environ[i], name, nlen) == 0 && environ[i][nlen] == '=')
      return i;
  }
  return -1;
}

char *getenv(const char *name) {
  if (!name || !*name) return 0;
  size_t nlen = strlen(name);
  int i = __environ_find(name, nlen);
  return i < 0 ? 0 : environ[i] + nlen + 1;
}

int setenv(const char *name, const char *value, int overwrite) {
  if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
  __environ_take_ownership();
  size_t nlen = strlen(name);
  int i = __environ_find(name, nlen);
  if (i >= 0) {
    if (!overwrite) return 0;
    char *e = __environ_entry(name, value);
    if (!e) { errno = ENOMEM; return -1; }
    __environ_dispose(environ[i]);
    environ[i] = e;
    __environ_mine_add(e);
    return 0;
  }
  int n = __environ_count();
  char **ne = realloc(environ, (n + 2) * sizeof(char *));
  if (!ne) { errno = ENOMEM; return -1; }
  environ = ne;
  char *e = __environ_entry(name, value);
  if (!e) { errno = ENOMEM; return -1; }
  environ[n] = e;
  environ[n + 1] = 0;
  __environ_mine_add(e);
  return 0;
}

int unsetenv(const char *name) {
  if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
  __environ_take_ownership();
  size_t nlen = strlen(name);
  int i;
  while ((i = __environ_find(name, nlen)) >= 0) {
    __environ_dispose(environ[i]);
    int j = i;
    do { environ[j] = environ[j + 1]; j++; } while (environ[j - 1]);
  }
  return 0;
}

/* putenv: POSIX — the caller's string ITSELF becomes part of the environment.
   No copy, and the libc never frees it (the caller owns it), so later edits
   to the buffer are visible through getenv(). putenv() of the exact pointer
   environ already holds is a no-op: busybox hush's PWD re-export at startup
   is precisely that call, and a copy+free here freed the buffer hush's
   variable store still pointed at (#296). A string lacking '=' removes that
   variable (glibc-compatible). */
int putenv(char *string) {
  char *eq = strchr(string, '=');
  if (!eq) return unsetenv(string);
  __environ_take_ownership();
  size_t nlen = eq - string;
  int i = __environ_find(string, nlen);
  if (i >= 0) {
    if (environ[i] != string) {
      __environ_dispose(environ[i]);
      environ[i] = string;
    }
    return 0;
  }
  int n = __environ_count();
  char **ne = realloc(environ, (n + 2) * sizeof(char *));
  if (!ne) { errno = ENOMEM; return -1; }
  environ = ne;
  environ[n] = string;
  environ[n + 1] = 0;
  return 0;
}

int clearenv(void) {
  if (__environ_owned) {
    for (int i = 0; environ[i]; i++) __environ_dispose(environ[i]);
    free(environ);
  }
  char **e = malloc(sizeof(char *));
  if (!e) { errno = ENOMEM; return -1; }
  e[0] = 0;
  environ = e;
  __environ_owned = 1;
  return 0;
}

/* Host boundary. __set_environ installs the initial environment (a
   NULL-terminated char** block built in wasm memory by host.js); __get_environ
   hands the live pointer back so JS can walk it (e.g. to inherit it into a
   spawned child). */
void __set_environ(char **envp) {
  environ = envp ? envp : __environ_empty;
  __environ_owned = 0;
  /* A stale registry entry could pointer-alias a future allocation; blank
     the slots (pre-main this is a no-op — the registry is still empty). */
  for (int i = 0; i < __environ_mine_cap; i++) __environ_mine[i] = 0;
}
char **__get_environ(void) { return environ; }
__export __set_environ = __set_environ;
__export __get_environ = __get_environ;

int system(const char *command) {
  /* posix_spawn("/bin/sh","-c",command) + waitpid. NOTE: needs /bin/sh on the
     image (the dash port) to actually run a command — until then __spawn finds
     no such program and this returns -1. A NULL command asks "is a shell
     available?": yes, once /bin/sh exists. */
  if (!command) return 1;
  char *argv[4];
  argv[0] = "sh"; argv[1] = "-c"; argv[2] = (char *)command; argv[3] = 0;
  pid_t pid;
  int e = posix_spawn(&pid, "/bin/sh", 0, 0, argv, 0);
  if (e != 0) { errno = e; return -1; }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return -1;
  return status;
}

/* UTF-8 primitives shared by the non-restartable conversions below and the
   restartable mbrtowc/wcrtomb in __wchar.c — C11 7.22.7 requires both
   families to describe the same execution-environment encoding. They live
   here (always linked) rather than in __wchar.c so plain stdlib users don't
   pull the whole wide-char library into the link. Return values follow
   mbrtowc/wcrtomb: (size_t)-1 invalid sequence, (size_t)-2 incomplete. */
size_t __mbrtowc_utf8(wchar_t *pwc, const char *s, size_t n) {
  if (!s) return 0;
  if (n == 0) return (size_t)-2;
  unsigned char b0 = (unsigned char)s[0];
  if (!__ctype_utf8) {
    if (pwc) *pwc = b0 < 128 ? b0 : 0xDF00 + b0;
    return b0 ? 1 : 0;
  }
  if (b0 < 0x80) {
    if (pwc) *pwc = b0;
    return b0 ? 1 : 0;
  }
  unsigned int cp; size_t len;
  if (b0 >= 0xC2 && b0 <= 0xDF) { cp = b0 & 0x1F; len = 2; }
  else if (b0 >= 0xE0 && b0 <= 0xEF) { cp = b0 & 0x0F; len = 3; }
  else if (b0 >= 0xF0 && b0 <= 0xF4) { cp = b0 & 0x07; len = 4; }
  else { errno = EILSEQ; return (size_t)-1; }
  for (size_t i = 1; i < len; i++) {
    if (i >= n) return (size_t)-2;
    unsigned char bi = (unsigned char)s[i];
    if ((bi & 0xC0) != 0x80 || (i == 1 &&
        ((b0 == 0xE0 && bi < 0xA0) || (b0 == 0xED && bi >= 0xA0) ||
         (b0 == 0xF0 && bi < 0x90) || (b0 == 0xF4 && bi >= 0x90)))) {
      errno = EILSEQ; return (size_t)-1;
    }
    cp = (cp << 6) | (bi & 0x3F);
  }
  if (pwc) *pwc = (wchar_t)cp;
  return cp ? len : 0;
}

size_t __wcrtomb_utf8(char *s, wchar_t wc) {
  unsigned int c = (unsigned int)wc;
  if (!s) return 1;
  if (!__ctype_utf8) {
    if (c < 128 || (c >= 0xDF80 && c <= 0xDFFF)) { s[0] = (char)c; return 1; }
    errno = EILSEQ; return (size_t)-1;
  }
  if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) { errno = EILSEQ; return (size_t)-1; }
  if (c < 0x80) { s[0] = (char)c; return 1; }
  if (c < 0x800) { s[0] = (char)(0xC0 | (c >> 6)); s[1] = (char)(0x80 | (c & 0x3F)); return 2; }
  if (c < 0x10000) { s[0] = (char)(0xE0 | (c >> 12)); s[1] = (char)(0x80 | ((c >> 6) & 0x3F)); s[2] = (char)(0x80 | (c & 0x3F)); return 3; }
  if (c < 0x110000) { s[0] = (char)(0xF0 | (c >> 18)); s[1] = (char)(0x80 | ((c >> 12) & 0x3F)); s[2] = (char)(0x80 | ((c >> 6) & 0x3F)); s[3] = (char)(0x80 | (c & 0x3F)); return 4; }
  return (size_t)-1;
}

/* Non-restartable multibyte conversions (C11 7.22.7), UTF-8 like the
   restartable family (the encoding is stateless — no shift sequences). */
int mblen(const char *s, size_t n) {
  return mbtowc((wchar_t *)0, s, n);
}

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
  if (!s) return 0;
  size_t r = __mbrtowc_utf8(pwc, s, n);
  if (r == (size_t)-1 || r == (size_t)-2) return -1;
  return (int)r;
}

int wctomb(char *s, wchar_t wc) {
  if (!s) return 0;
  size_t r = __wcrtomb_utf8(s, wc);
  return r == (size_t)-1 ? -1 : (int)r;
}

size_t mbstowcs(wchar_t *dest, const char *src, size_t n) {
  size_t out = 0;
  while (!dest || out < n) {
    wchar_t wc = 0;
    int k = mbtowc(&wc, src, MB_CUR_MAX);
    if (k < 0) return (size_t)-1;
    if (dest) dest[out] = wc;
    if (k == 0) return out;  /* null wide char stored, not counted */
    out++;
    src += k;
  }
  return out;
}

size_t wcstombs(char *dest, const wchar_t *src, size_t n) {
  size_t out = 0;
  for (size_t i = 0; ; i++) {
    char tmp[4];
    int k = wctomb(tmp, src[i]);
    if (k < 0) return (size_t)-1;
    if (src[i] == 0) {
      if (dest && out < n) dest[out] = '\0';
      return out;  /* null byte stored, not counted */
    }
    if (dest && out + (size_t)k > n) return out;  /* no room for a whole char */
    if (dest) for (int j = 0; j < k; j++) dest[out + j] = tmp[j];
    out += k;
  }
}
  