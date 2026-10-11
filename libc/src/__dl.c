
#include <dlfcn.h>
__import void *__dl_open(const char *, int);
__import void *__dl_symbol(void *, const char *);
__import int __dl_close(void *);
__import int __dl_error(char *, unsigned);
static _Thread_local char dl_error_buffer[512];
void *dlopen(const char *path, int flags) { return __dl_open(path, flags); }
void *dlsym(void *handle, const char *name) { return __dl_symbol(handle, name); }
int dlclose(void *handle) { return __dl_close(handle); }
char *dlerror(void) { return __dl_error(dl_error_buffer, sizeof(dl_error_buffer)) ? dl_error_buffer : (void *)0; }
  