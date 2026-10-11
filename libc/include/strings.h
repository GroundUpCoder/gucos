
#pragma once
__require_source("__strings.c");
#include <stddef.h>
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strcasestr(const char *haystack, const char *needle);
int ffs(int x);
int ffsl(long x);
int ffsll(long long x);
int fls(int x);
int flsl(long x);
int flsll(long long x);
  