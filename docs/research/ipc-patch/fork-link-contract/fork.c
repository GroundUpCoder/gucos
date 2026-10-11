#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
int invoke(int (*fn)(void));static int cb(void){int p=fork();if(p<0)return -1;if(!p)_exit(0);int s;if(waitpid(p,&s,0)!=p||!WIFEXITED(s)||WEXITSTATUS(s))return -2;return 42;}int main(void){int x=invoke(cb);printf("FORK-%d\n",x);return x!=42;}
