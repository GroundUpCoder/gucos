#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static int cleaned;
static void cleanup(void *arg){cleaned=cleaned*10+(int)(long)arg;}
static void *worker(void *arg){pthread_cleanup_push(cleanup,(void *)1);pthread_cleanup_push(cleanup,(void *)2);pthread_exit((void *)42);pthread_cleanup_pop(0);pthread_cleanup_pop(0);return 0;}
int main(void){
 mkdir("/tmp/at-original",0700);int d=open("/tmp/at-original",O_RDONLY|O_DIRECTORY);CHECK(d>=0);
 CHECK(rename("/tmp/at-original","/tmp/at-renamed")==0);
 int f=openat(d,"file",O_CREAT|O_RDWR,0600);CHECK(f>=0);CHECK(write(f,"abc",3)==3);
 struct stat st;CHECK(fstatat(d,"file",&st,0)==0&&st.st_size==3);
 CHECK(mkdirat(d,"sub",0700)==0);int sub=openat(d,"sub",O_RDONLY|O_DIRECTORY);CHECK(sub>=0);
 CHECK(renameat(d,"file",sub,"moved")==0);CHECK(fstatat(sub,"moved",&st,0)==0);
 CHECK(symlinkat("moved",sub,"sym")==0);char buf[20]={0};CHECK(readlinkat(sub,"sym",buf,20)==5&&!memcmp(buf,"moved",5));
 CHECK(linkat(sub,"sym",d,"hard",AT_SYMLINK_FOLLOW)==0);CHECK(fstatat(d,"hard",&st,0)==0&&S_ISREG(st.st_mode));
 CHECK(fstatat(sub,"sym",&st,AT_SYMLINK_NOFOLLOW)==0&&S_ISLNK(st.st_mode));
 CHECK(openat(f,"bad",O_RDONLY)<0&&errno==ENOTDIR);CHECK(openat(-99,"bad",O_RDONLY)<0&&errno==EBADF);
 CHECK(fstatat(-99,"/tmp/at-renamed/hard",&st,0)==0);
 CHECK(fstatat(d,"hard",&st,0x40000000)<0&&errno==EINVAL);
 CHECK(faccessat(d,"hard",X_OK,0)<0&&errno==EACCES);CHECK(faccessat(d,"hard",8,0)<0&&errno==EINVAL);CHECK(faccessat(d,"hard",F_OK,0)==0);CHECK(fchmodat(d,"hard",0644,0)==0);
 CHECK(unlinkat(sub,"sym",0)==0);CHECK(unlinkat(sub,"moved",0)==0);CHECK(unlinkat(d,"sub",AT_REMOVEDIR)==0);CHECK(unlinkat(d,"hard",0)==0);
 CHECK(pthread_cancel(pthread_self())==ENOTSUP);int old=73;CHECK(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&old)==ENOTSUP&&old==73);CHECK(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&old)==ENOTSUP&&old==73);
 CHECK(pthread_setcancelstate(99,&old)==EINVAL);pthread_mutexattr_t a;pthread_mutexattr_init(&a);CHECK(pthread_mutexattr_setpshared(&a,PTHREAD_PROCESS_SHARED)==ENOTSUP);
 CHECK(chown("/tmp/no-such-owner-file",0,0)<0&&errno==ENOENT);CHECK(fchown(-99,0,0)<0&&errno==EBADF);CHECK(fchown(f,1,0)<0&&errno==ENOTSUP);
 CHECK(setuid(1)<0&&errno==ENOTSUP);pthread_t t;void *ret;CHECK(pthread_create(&t,0,worker,0)==0);CHECK(pthread_join(t,&ret)==0);CHECK(ret==(void *)42&&cleaned==21);
 puts("PLATFORM-POSIX-PASS");return 0;
}
