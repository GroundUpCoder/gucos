
#include <stddef.h>

static int __tolower(int c) {
  if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
  return c;
}

int strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    int c1 = __tolower((unsigned char)*s1);
    int c2 = __tolower((unsigned char)*s2);
    if (c1 != c2) return c1 - c2;
    s1++;
    s2++;
  }
  return __tolower((unsigned char)*s1) - __tolower((unsigned char)*s2);
}

char *strcasestr(const char *haystack, const char *needle) {
  if (!*needle) return (char *)haystack;
  for (; *haystack; haystack++) {
    const char *h = haystack, *n = needle;
    while (*h && *n && __tolower((unsigned char)*h) == __tolower((unsigned char)*n)) { h++; n++; }
    if (!*n) return (char *)haystack;
  }
  return 0;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    int c1 = __tolower((unsigned char)*s1);
    int c2 = __tolower((unsigned char)*s2);
    if (c1 != c2) return c1 - c2;
    if (*s1 == '\0') return 0;
    s1++;
    s2++;
  }
  return 0;
}

int ffs(int x) { return x ? __wasm(int, (x), op 0x68) + 1 : 0; }
int ffsl(long x) { return x ? __wasm(int, (x), op 0x68) + 1 : 0; }
int ffsll(long long x) { return x ? (int)__wasm(long long, (x), op 0x7A) + 1 : 0; }
int fls(int x) { return x ? 32 - __wasm(int, (x), op 0x67) : 0; }
int flsl(long x) { return x ? 32 - __wasm(int, (x), op 0x67) : 0; }
int flsll(long long x) { return x ? 64 - (int)__wasm(long long, (x), op 0x79) : 0; }
  