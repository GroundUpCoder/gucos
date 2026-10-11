
#pragma once
static inline char *strsep(char **stringp, const char *delim) {
 char *start=*stringp;if(!start)return 0;
 for(char *p=start;;p++){
  if(!*p){*stringp=0;return start;}
  for(const char *d=delim;*d;d++)if(*p==*d){*p=0;*stringp=p+1;return start;}
 }
}

__require_source("__string.c");
#include <stddef.h>
/* glibc (__USE_MISC) and musl (_BSD_SOURCE/_GNU_SOURCE/_XOPEN_SOURCE, i.e.
   every default configuration) make <string.h> pull in <strings.h>, so
   portable code reaches strcasecmp/strncasecmp/ffs without naming the second
   header. libzip is one such consumer and had to be shimmed for it during the
   docs/archive/0350 zip measurement (docs/archive/0382 gap 3). Match the platforms code is
   actually written against. */
#include <strings.h>
#define NULL ((void *)0)
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strchr(const char *s, int c);
char *strchrnul(const char *s, int c);
#define __strchrnul strchrnul
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
void *memchr(const void *s, int c, size_t n);
char *strncat(char *dest, const char *src, size_t n);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);
int strcoll(const char *s1, const char *s2);
size_t strxfrm(char *dest, const char *src, size_t n);
char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t buflen);
char *strdup(const char *s);
char *strndup(const char *s, size_t maxlen);
/* docs/archive/0325 Group B. */
void *memrchr(const void *s, int c, size_t n);
/* explicit_bzero: a memset(0) the compiler is not allowed to elide as a
   dead store. _blake2's only alternative is a GCC inline-asm memory
   barrier, which this compiler also lacks. */
void explicit_bzero(void *s, size_t n);
/* strsignal: the signal's description, for signalmodule.c. */
char *strsignal(int sig);
  