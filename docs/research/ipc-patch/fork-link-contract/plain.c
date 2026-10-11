#include <stdio.h>
int invoke(int (*fn)(void));static int cb(void){return 42;}int main(void){int x=invoke(cb);printf("PLAIN-%d\n",x);return x!=42;}
