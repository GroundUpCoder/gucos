
#pragma once
__require_source("__progname.c");
const char *getprogname(void);
void setprogname(const char *name);

__require_source("__stdlib.c");
#include <stddef.h>
#include <__atexit.h>
#include <__malloc.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

int abs(int n);
long labs(long n);

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
__require_source_if("strtod", "__scan.c");
__require_source_if("strtof", "__scan.c");
__require_source_if("strtold", "__scan.c");
double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
long double strtold(const char *nptr, char **endptr);
double atof(const char *nptr);
long long llabs(long long n);
int rand(void);
void srand(unsigned int seed);
/* BSD/XSI random family (ticket #112) — musl's lagged Fibonacci generator.
   31-bit outputs; the default sequence equals srandom(1)'s. */
long random(void);
void srandom(unsigned seed);
char *initstate(unsigned seed, char *state, size_t size);
char *setstate(char *state);
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void exit(int status);
void abort(void);

char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
int clearenv(void);
int system(const char *command);
int mkstemp(char *template_);
char *mktemp(char *template_);
char *mkdtemp(char *template_);

__require_source("__locale.c");
extern int __ctype_utf8;
__require_source("__pty.c");
int posix_openpt(int);
int grantpt(int);
int unlockpt(int);
char *ptsname(int);
int ptsname_r(int, char *, size_t);
#define MB_CUR_MAX (__ctype_utf8 ? 4 : 1)
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);
  