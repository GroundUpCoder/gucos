
#pragma once
#include <limits.h>
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define NBBY 8
#define MAXSYMLINKS 20
#define howmany(x, y)  (((x) + ((y) - 1)) / (y))
#define roundup(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))
#define powerof2(x)    ((((x) - 1) & (x)) == 0)
  