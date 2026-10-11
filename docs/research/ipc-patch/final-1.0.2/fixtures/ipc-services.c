#include <sys/file.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d errno %d\n",__LINE__,errno);exit(1);}}while(0)
static volatile sig_atomic_t signaled;
static void handler(int n){(void)n;signaled=1;}
static sem_t unnamed;
static void *post_thread(void *arg){CHECK(sem_post(arg)==0);return 0;}
static void child_ok(pid_t p){int s;CHECK(waitpid(p,&s,0)==p);CHECK(WIFEXITED(s)&&WEXITSTATUS(s)==0);}
static struct timespec future(void){struct timespec t;CHECK(clock_gettime(CLOCK_REALTIME,&t)==0);t.tv_nsec+=50000000;if(t.tv_nsec>=1000000000){t.tv_sec++;t.tv_nsec-=1000000000;}return t;}
int main(int argc,char **argv){
  if(argc>1&&!strcmp(argv[1],"exec")){
    CHECK(argc==4);
    int inherited=atoi(argv[2]),cloexec=atoi(argv[3]);
    CHECK(fcntl(cloexec,F_GETFD)<0&&errno==EBADF);
    CHECK(flock(inherited,LOCK_EX|LOCK_NB)==0);
    int competitor=open("/tmp/ipc-lock",O_RDWR);CHECK(competitor>=0);
    CHECK(flock(competitor,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);close(competitor);
    sem_t *s=sem_open("/ipc-exec",0);CHECK(s!=SEM_FAILED);CHECK(sem_post(s)==0);CHECK(sem_close(s)==0);return 0;
  }
  CHECK(sizeof(sem_t)==4);
  int a=open("/tmp/ipc-lock",O_CREAT|O_RDWR,0600),b=open("/tmp/ipc-lock",O_RDWR);CHECK(a>=0&&b>=0);
  CHECK(flock(a,LOCK_EX)==0);CHECK(flock(b,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);
  int d=dup(a);CHECK(d>=0);CHECK(close(a)==0);CHECK(flock(b,LOCK_SH|LOCK_NB)<0&&errno==EWOULDBLOCK);
  /* fcntl locks are a separate namespace, and closing another OFD does not
     release this flock. Conversion releases the old shared lock on failure. */
  struct flock rec={0};rec.l_type=F_WRLCK;rec.l_whence=SEEK_SET;CHECK(fcntl(b,F_SETLK,&rec)==0);
  CHECK(flock(d,LOCK_SH)==0);CHECK(flock(b,LOCK_SH)==0);
  CHECK(flock(d,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);
  CHECK(flock(b,LOCK_EX|LOCK_NB)==0);CHECK(flock(b,LOCK_UN)==0);
  CHECK(flock(d,LOCK_EX)==0);
  int gate[2];CHECK(pipe(gate)==0);
  pid_t p=fork();CHECK(p>=0);
  if(!p){close(gate[1]);char c;CHECK(read(gate[0],&c,1)==1);CHECK(flock(b,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);CHECK(close(d)==0);CHECK(flock(b,LOCK_EX)==0);_exit(0);}
  CHECK(close(d)==0);CHECK(write(gate[1],"x",1)==1);child_ok(p);
  CHECK(flock(b,LOCK_EX|LOCK_NB)==0);CHECK(flock(b,LOCK_UN)==0);close(b);close(gate[0]);close(gate[1]);
  CHECK(flock(-1,LOCK_EX)<0&&errno==EBADF);
  a=open("/tmp/ipc-lock",O_RDWR);b=open("/tmp/ipc-lock",O_RDWR);CHECK(a>=0&&b>=0);
  CHECK(flock(a,LOCK_EX)==0);CHECK(pipe(gate)==0);p=fork();CHECK(p>=0);
  if(!p){
    close(a);struct sigaction sa;memset(&sa,0,sizeof sa);sa.sa_handler=handler;sigemptyset(&sa.sa_mask);
    CHECK(sigaction(SIGALRM,&sa,0)==0);alarm(1);
    CHECK(flock(b,LOCK_EX)<0&&errno==EINTR&&signaled);alarm(0);
    CHECK(write(gate[1],"x",1)==1);CHECK(flock(b,LOCK_EX)==0);_exit(0);
  }
  char ready;CHECK(read(gate[0],&ready,1)==1);usleep(20000);
  CHECK(flock(a,LOCK_UN)==0);child_ok(p);close(a);close(b);close(gate[0]);close(gate[1]);
  puts("flock ownership/conversion/fork PASS");

  int e=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);CHECK(e>=0);
  CHECK(fcntl(e,F_GETFD)==FD_CLOEXEC);CHECK((fcntl(e,F_GETFL)&(O_RDWR|O_NONBLOCK))==(O_RDWR|O_NONBLOCK));
  struct pollfd poller={e,POLLIN|POLLOUT,0};CHECK(poll(&poller,1,0)==1&&poller.revents==POLLOUT);
  eventfd_t v=0;CHECK(eventfd_read(e,&v)<0&&errno==EAGAIN);
  CHECK(eventfd_write(e,0xfffffffffffffffeULL)==0);
  CHECK(poll(&poller,1,0)==1&&poller.revents==POLLIN);
  CHECK(eventfd_write(e,1)<0&&errno==EAGAIN);CHECK(eventfd_write(e,~0ULL)<0&&errno==EINVAL);
  char tiny[7];CHECK(read(e,tiny,7)<0&&errno==EINVAL);
  CHECK(write(e,tiny,7)<0&&errno==EINVAL);
  CHECK(read(e,tiny,0)<0&&errno==EINVAL);
  CHECK(eventfd(0,0x4000)<0&&errno==EINVAL);
  CHECK(eventfd_read(e,&v)==0&&v==0xfffffffffffffffeULL);
  CHECK(eventfd_write(e,0x100000001ULL)==0);d=dup(e);CHECK(close(e)==0);
  CHECK(eventfd_read(d,&v)==0&&v==0x100000001ULL);close(d);
  e=eventfd(2,EFD_SEMAPHORE|EFD_NONBLOCK);CHECK(e>=0);
  CHECK(eventfd_read(e,&v)==0&&v==1);CHECK(eventfd_read(e,&v)==0&&v==1);
  CHECK(eventfd_read(e,&v)<0&&errno==EAGAIN);close(e);
  e=eventfd(0,0);CHECK(e>=0);p=fork();CHECK(p>=0);
  if(!p){CHECK(eventfd_write(e,19)==0);_exit(0);}
  poller.fd=e;poller.events=POLLIN;CHECK(poll(&poller,1,2000)==1&&(poller.revents&POLLIN));
  CHECK(eventfd_read(e,&v)==0&&v==19);child_ok(p);close(e);
  /* Blocking writer wakes after a drain; signal cancellation must not
     consume a later token or leave a stale waiter. */
  e=eventfd(0,0);CHECK(e>=0);CHECK(eventfd_write(e,0xfffffffffffffffeULL)==0);
  p=fork();CHECK(p>=0);if(!p){CHECK(eventfd_write(e,1)==0);_exit(0);}
  usleep(20000);CHECK(eventfd_read(e,&v)==0&&v==0xfffffffffffffffeULL);
  child_ok(p);CHECK(eventfd_read(e,&v)==0&&v==1);
  struct sigaction sa;memset(&sa,0,sizeof sa);sa.sa_handler=handler;sigemptyset(&sa.sa_mask);
  CHECK(sigaction(SIGALRM,&sa,0)==0);alarm(1);CHECK(eventfd_read(e,&v)<0&&errno==EINTR&&signaled);alarm(0);
  CHECK(eventfd_write(e,7)==0);CHECK(eventfd_read(e,&v)==0&&v==7);close(e);
  puts("eventfd counter/readiness/blocking/signal/fork PASS");

  sem_unlink("/ipc-named");
  sem_t *s=sem_open("/ipc-named",O_CREAT|O_EXCL,0600,0);CHECK(s!=SEM_FAILED);
  sem_t *same=sem_open("/ipc-named",0);CHECK(same==s);
  CHECK(sem_open("/ipc-named",O_CREAT|O_EXCL,0600,1)==SEM_FAILED&&errno==EEXIST);
  CHECK(sem_trywait(s)<0&&errno==EAGAIN);
  struct timespec bad={0,1000000000};CHECK(sem_timedwait(s,&bad)<0&&errno==EINVAL);
  struct timespec t=future();CHECK(sem_timedwait(s,&t)<0&&errno==ETIMEDOUT);
  signaled=0;alarm(1);CHECK(sem_wait(s)<0&&errno==EINTR&&signaled);alarm(0);
  CHECK(sem_post(s)==0);CHECK(sem_trywait(s)==0);
  pthread_t th;CHECK(pthread_create(&th,0,post_thread,s)==0);CHECK(sem_wait(s)==0);CHECK(pthread_join(th,0)==0);
  p=fork();CHECK(p>=0);
  if(!p){CHECK(sem_post(s)==0);CHECK(sem_close(s)==0);CHECK(sem_close(same)==0);_exit(0);}
  CHECK(sem_wait(s)==0);child_ok(p);
  CHECK(sem_unlink("/ipc-named")==0);
  sem_t *fresh=sem_open("/ipc-named",O_CREAT|O_EXCL,0600,3);CHECK(fresh!=SEM_FAILED&&fresh!=s);
  int value;CHECK(sem_getvalue(fresh,&value)==0&&value==3);CHECK(sem_getvalue(s,&value)==0&&value==0);
  CHECK(sem_close(same)==0);CHECK(sem_post(s)==0);CHECK(sem_wait(s)==0);CHECK(sem_close(s)==0);
  CHECK(sem_close(fresh)==0);CHECK(sem_unlink("/ipc-named")==0);
  CHECK(sem_open("/ipc-named",0)==SEM_FAILED&&errno==ENOENT);
  s=sem_open("/ipc-named",O_CREAT|O_EXCL,0600,SEM_VALUE_MAX);CHECK(s!=SEM_FAILED);
  CHECK(sem_post(s)<0&&errno==EOVERFLOW);CHECK(sem_close(s)==0);CHECK(sem_unlink("/ipc-named")==0);
  s=sem_open("/ipc-exec",O_CREAT|O_EXCL,0600,0);CHECK(s!=SEM_FAILED);
  a=open("/tmp/ipc-lock",O_RDWR);CHECK(a>=0);CHECK(flock(a,LOCK_EX)==0);
  e=eventfd(0,EFD_CLOEXEC);CHECK(e>=0);
  char af[20],ef[20];snprintf(af,sizeof af,"%d",a);snprintf(ef,sizeof ef,"%d",e);
  p=fork();CHECK(p>=0);if(!p){execl("/tmp/ipc-services","ipc-services","exec",af,ef,(char *)0);_exit(99);}
  close(a);close(e);
  CHECK(sem_wait(s)==0);child_ok(p);CHECK(sem_close(s)==0);CHECK(sem_unlink("/ipc-exec")==0);
  puts("named semaphore identity/unlink/fork/exec/timeout PASS");

  CHECK(sem_init(&unnamed,0,0)==0);CHECK(pthread_create(&th,0,post_thread,&unnamed)==0);
  CHECK(sem_wait(&unnamed)==0);CHECK(pthread_join(th,0)==0);
  t=future();CHECK(sem_timedwait(&unnamed,&t)<0&&errno==ETIMEDOUT);
  CHECK(sem_post(&unnamed)==0);t.tv_sec=0;t.tv_nsec=0;CHECK(sem_timedwait(&unnamed,&t)==0);
  CHECK(sem_init(&unnamed,1,0)<0&&errno==ENOSYS);
  CHECK(sem_destroy(&unnamed)==0);
  puts("IPC-SERVICES-PASS");return 0;
}
