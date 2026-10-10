
#pragma once
__require_source("__dl.c");
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 256
#define RTLD_LOCAL 0
#define RTLD_NODELETE 4096
#define RTLD_NOLOAD 4
#define RTLD_DEFAULT ((void *)0)
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
int dlclose(void *);
char *dlerror(void);
  