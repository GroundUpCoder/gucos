
#pragma once
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>    /* wide stream I/O takes FILE * */
#include <wctype.h>
__require_source("__wchar.c");
__require_source("__stdlib.c");  /* mbrtowc/wcrtomb use its shared UTF-8 codec */
typedef struct { int __state; } mbstate_t;
size_t wcslen(const wchar_t *s);
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
wchar_t *wcsncat(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);
size_t wcsspn(const wchar_t *s, const wchar_t *accept);
size_t wcscspn(const wchar_t *s, const wchar_t *reject);
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept);
wchar_t *wcstok(wchar_t *str, const wchar_t *delim, wchar_t **saveptr);
int wcscoll(const wchar_t *s1, const wchar_t *s2);
size_t wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemset(wchar_t *dest, wchar_t c, size_t n);
int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
/* Wide numeric conversion (C95 7.24.4.1). wcstol is docs/archive/0325 Group A —
   CPython's initconfig.c calls it unconditionally; the rest of the family
   comes with it because they are one scanner. */
long wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long wcstoul(const wchar_t *nptr, wchar_t **endptr, int base);
long long wcstoll(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long long wcstoull(const wchar_t *nptr, wchar_t **endptr, int base);
double wcstod(const wchar_t *nptr, wchar_t **endptr);
/* wcsftime (docs/archive/0325 Group B): strftime's wide twin, for timemodule.c. */
struct tm;
size_t wcsftime(wchar_t *dst, size_t maxsize, const wchar_t *fmt, const struct tm *tm);
/* Wide stream I/O (C95 7.24.3, ticket #115): UTF-8 over the byte streams.
   Field widths and %n in the wide scanf family count BYTES for multi-byte
   input (exact for all single-byte characters — the C locale's world).
   fwprintf/fwide are deliberately absent, not stubbed. */
wint_t fgetwc(FILE *stream);
wint_t getwc(FILE *stream);
wint_t getwchar(void);
wint_t ungetwc(wint_t c, FILE *stream);
wint_t fputwc(wchar_t c, FILE *stream);
wint_t putwc(wchar_t c, FILE *stream);
wint_t putwchar(wchar_t c);
wchar_t *fgetws(wchar_t *ws, int n, FILE *stream);
int fputws(const wchar_t *ws, FILE *stream);
int fwscanf(FILE *stream, const wchar_t *fmt, ...);
int wscanf(const wchar_t *fmt, ...);
int swscanf(const wchar_t *s, const wchar_t *fmt, ...);
int vfwscanf(FILE *stream, const wchar_t *fmt, va_list ap);
int vwscanf(const wchar_t *fmt, va_list ap);
int vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap);

size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps);
size_t wcsnrtombs(char *dst, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps);
size_t mbsnrtowcs(wchar_t *dst, const char **src, size_t nms, size_t len, mbstate_t *ps);
size_t wcsnlen(const wchar_t *s, size_t n);
wchar_t *wcsdup(const wchar_t *s);
int wcscasecmp(const wchar_t *a, const wchar_t *b);
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n);
int wcwidth(wchar_t wc);
int wcswidth(const wchar_t *s, size_t n);
