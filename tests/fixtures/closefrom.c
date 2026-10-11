
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define CHECK(x) do { if(!(x)){printf("FAIL closefrom line %d errno %d\n",__LINE__,errno);exit(1);} } while(0)
#define HIGH 1000007
static void dead(int fd){char c;CHECK(read(fd,&c,1)<0&&errno==EBADF);CHECK(fcntl(fd,F_GETFD)<0&&errno==EBADF);}
static void *closer(void *unused){(void)unused;closefrom(HIGH);return 0;}
int main(void){
 int a=open("/tmp/closefrom-lock",O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(a>=3);
 CHECK(write(a,"abc",3)==3);CHECK(lseek(a,0,SEEK_SET)==0);
 CHECK(dup2(a,HIGH)==HIGH);CHECK(dup2(a,HIGH+23)==HIGH+23);
 CHECK(fcntl(HIGH,F_SETFD,FD_CLOEXEC)==0);
 int p=fork();CHECK(p>=0);
 if(!p){closefrom(HIGH);dead(HIGH);dead(HIGH+23);CHECK(fcntl(a,F_GETFD)>=0);_exit(0);}
 int status;CHECK(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 CHECK(fcntl(HIGH,F_GETFD)==FD_CLOEXEC);CHECK(fcntl(HIGH+23,F_GETFD)>=0);
 CHECK(flock(a,LOCK_EX)==0);
 closefrom(HIGH);dead(HIGH);dead(HIGH+23);
 int b=open("/tmp/closefrom-lock",O_RDWR);CHECK(b>=0);
 CHECK(flock(b,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);
 CHECK(dup2(a,HIGH)==HIGH);CHECK(close(a)==0);
 closefrom(HIGH);CHECK(flock(b,LOCK_EX|LOCK_NB)==0);CHECK(close(b)==0);
 puts("CLOSEFROM sparse/fork/flock PASS");
 int pipefd[2];CHECK(pipe(pipefd)==0);CHECK(dup2(pipefd[1],HIGH)==HIGH);CHECK(close(pipefd[1])==0);
 closefrom(HIGH);char c;CHECK(read(pipefd[0],&c,1)==0);CHECK(close(pipefd[0])==0);
 puts("CLOSEFROM pipe EOF PASS");
 a=open("/tmp/closefrom-lock",O_RDONLY);CHECK(a>=0);
 CHECK(dup2(a,HIGH)==HIGH);
 pthread_t t;CHECK(pthread_create(&t,0,closer,0)==0);CHECK(pthread_join(t,0)==0);
 dead(HIGH);CHECK(read(a,&c,1)==1&&c=='a');CHECK(close(a)==0);
 p=fork();CHECK(p>=0);
 if(!p){closefrom(-1);dead(0);dead(1);dead(2);_exit(0);}
 CHECK(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 puts("CLOSEFROM thread/negative PASS");
 puts("CLOSEFROM-PASS");return 0;
}
