#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"ipc line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}}while(0)
static ssize_t put(int s,int *fds,int nf,const void *data,size_t len){
 char control[CMSG_SPACE(sizeof(int)*4)]={0};
 struct iovec v={(void *)data,len};struct msghdr m={0};m.msg_iov=&v;m.msg_iovlen=1;
 if(nf){m.msg_control=control;m.msg_controllen=CMSG_SPACE(sizeof(int)*nf);
 struct cmsghdr *c=CMSG_FIRSTHDR(&m);c->cmsg_len=CMSG_LEN(sizeof(int)*nf);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;memcpy(CMSG_DATA(c),fds,nf*sizeof(int));}
 return sendmsg(s,&m,0);
}
static ssize_t get(int s,int *fds,int cap,char *data,size_t len,int flags,int *outflags){
 char control[CMSG_SPACE(sizeof(int)*4)]={0};
 struct iovec v={data,len};struct msghdr m={0};m.msg_iov=&v;m.msg_iovlen=1;
 if(cap){m.msg_control=control;m.msg_controllen=CMSG_LEN(sizeof(int)*cap);}
 ssize_t n=recvmsg(s,&m,flags);if(outflags)*outflags=m.msg_flags;
 if(n>=0&&cap){struct cmsghdr *c=CMSG_FIRSTHDR(&m);CHECK(c&&c->cmsg_level==SOL_SOCKET&&c->cmsg_type==SCM_RIGHTS);
 int nf=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);CHECK(nf<=cap);memcpy(fds,CMSG_DATA(c),nf*sizeof(int));}
 return n;
}
int main(int argc,char **argv){
 if(argc==3&&!strcmp(argv[1],"--closed")){CHECK(fcntl(atoi(argv[2]),F_GETFD)==-1&&errno==EBADF);return 0;}
 int s[2],p[2],f,got[4],flags;char buf[32]={0};
 CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,s)==0);
 f=open("ipc-offset",O_CREAT|O_RDWR|O_TRUNC,0600);CHECK(f>=0);CHECK(write(f,"abcdef",6)==6);CHECK(lseek(f,0,SEEK_SET)==0);
 CHECK(put(s[0],&f,1,"x",1)==1);
 CHECK(get(s[1],got,1,buf,1,MSG_PEEK|MSG_CMSG_CLOEXEC,&flags)==1);
 CHECK(fcntl(got[0],F_GETFD)==FD_CLOEXEC);CHECK(read(got[0],buf,2)==2&&!memcmp(buf,"ab",2));close(got[0]);
 close(f);
 CHECK(get(s[1],got,1,buf,1,0,&flags)==1);CHECK(fcntl(got[0],F_GETFD)==0);
 CHECK(read(got[0],buf,2)==2&&!memcmp(buf,"cd",2));close(got[0]);
 // Plain read must dispose the queued pipe writer, observable as EOF.
 CHECK(pipe(p)==0);CHECK(put(s[0],&p[1],1,"z",1)==1);close(p[1]);CHECK(read(s[1],buf,1)==1);
 CHECK(fcntl(p[0],F_SETFL,O_NONBLOCK)==0);CHECK(read(p[0],buf,1)==0);close(p[0]);
 CHECK(pipe(p)==0);int twice[2]={p[1],p[1]};CHECK(put(s[0],twice,2,"t",1)==1);close(p[1]);
 CHECK(get(s[1],got,1,buf,1,0,&flags)==1);CHECK(flags&MSG_CTRUNC);close(got[0]);CHECK(read(p[0],buf,1)==0);close(p[0]);
 // A received close-on-exec descriptor must disappear at actual exec.
 f=open("/dev/null",O_RDONLY);CHECK(f>=0);CHECK(put(s[0],&f,1,"e",1)==1);close(f);
 CHECK(get(s[1],got,1,buf,1,MSG_CMSG_CLOEXEC,&flags)==1);
 pid_t child=fork();CHECK(child>=0);
 if(!child){char fd[20];snprintf(fd,sizeof(fd),"%d",got[0]);execl(argv[0],argv[0],"--closed",fd,(char *)0);_exit(99);}
 int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);close(got[0]);
 // Separate process owns and sends a file opened after fork.
 child=fork();CHECK(child>=0);
 if(!child){close(s[1]);int fd=open("ipc-offset",O_RDONLY);CHECK(fd>=0);CHECK(put(s[0],&fd,1,"f",1)==1);close(fd);_exit(0);}
 CHECK(get(s[1],got,1,buf,1,0,&flags)==1);CHECK(read(got[0],buf,6)==6&&!memcmp(buf,"abcdef",6));close(got[0]);
 CHECK(waitpid(child,&status,0)==child&&WEXITSTATUS(status)==0);close(s[0]);close(s[1]);unlink("ipc-offset");
 for(int type=2;type<=5;type+=3){
  CHECK(socketpair(AF_UNIX,type|SOCK_NONBLOCK,0,s)==0);
  CHECK(send(s[0],"abc",3,0)==3);CHECK(send(s[0],"de",2,0)==2);
  CHECK(get(s[1],got,0,buf,1,MSG_TRUNC,&flags)==3);CHECK(flags&MSG_TRUNC);
  CHECK(recv(s[1],buf,sizeof(buf),0)==2&&!memcmp(buf,"de",2));
  CHECK(send(s[0],"",0,0)==0);CHECK(recv(s[1],buf,0,0)==0);
  CHECK(recv(s[1],buf,1,0)==-1&&errno==EAGAIN);close(s[0]);close(s[1]);
 }
 struct sockaddr_un a={0},b={0},from={0};a.sun_family=b.sun_family=AF_UNIX;
 strcpy(a.sun_path,"ipc-dgram-a");strcpy(b.sun_path,"ipc-dgram-b");unlink(a.sun_path);unlink(b.sun_path);
 s[0]=socket(AF_UNIX,SOCK_DGRAM,0);s[1]=socket(AF_UNIX,SOCK_DGRAM,0);CHECK(s[0]>=0&&s[1]>=0);
 CHECK(bind(s[0],(struct sockaddr *)&a,sizeof(a))==0);CHECK(bind(s[1],(struct sockaddr *)&b,sizeof(b))==0);
 CHECK(sendto(s[0],"hello",5,0,(struct sockaddr *)&b,sizeof(b))==5);socklen_t size=sizeof(from);
 CHECK(recvfrom(s[1],buf,sizeof(buf),0,(struct sockaddr *)&from,&size)==5);CHECK(!strcmp(from.sun_path,a.sun_path));
 CHECK(connect(s[0],(struct sockaddr *)&b,sizeof(b))==0);CHECK(send(s[0],"!",1,0)==1);CHECK(recv(s[1],buf,1,0)==1);
 close(s[0]);close(s[1]);unlink(a.sun_path);unlink(b.sun_path);
 puts("IPC-SOCKET-PASS");return 0;
}

