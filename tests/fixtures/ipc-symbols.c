#include <dlfcn.h>
#include <stdio.h>
int main(void){
 const char *names[]={"flock","eventfd","eventfd_read","eventfd_write","sem_open","sem_close","sem_unlink","sem_timedwait","sem_wait","sem_post",0};
 void *lib=dlopen("libc.so.1",RTLD_NOW);if(!lib){puts(dlerror());return 1;}
 for(int i=0;names[i];i++)if(!dlsym(lib,names[i])){printf("missing %s: %s\n",names[i],dlerror());return 2;}
 puts("IPC-SYMBOLS-PASS");return 0;
}
