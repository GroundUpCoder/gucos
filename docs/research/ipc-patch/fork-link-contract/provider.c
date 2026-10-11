#include <stdio.h>
int invoke(int (*fn)(void)){fprintf(stderr,"PROVIDER\n");return fn();}
