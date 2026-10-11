
#include <dlfcn.h>
#include <stdio.h>
int main(void){
 void *lib=dlopen("libc.so.1",RTLD_NOW);if(!lib){puts(dlerror());return 1;}
 void (*fn)(int)=dlsym(lib,"closefrom");if(!fn){puts(dlerror());return 2;}
 fn(2000000000);puts("CLOSEFROM-SYMBOL-PASS");return 0;
}
