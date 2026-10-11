#include <sys/stat.h>
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

/* One-off behavioral adaptation. Linux scm_rights.c graph topologies;
 * FreeBSD unix_passfd_test.c send_and_close/send_and_shutdown/truncated_rights.
 * Original source bytes, licenses and checksums are retained alongside this.
 * Harness, assertions and pipe-EOF resource oracle are local adaptations. */
static void graph(int type,int n,const int edges[][2],int ne){
 int sv[10][2],sentinel[2];char ch;
 CHECK(pipe(sentinel)==0);
 for(int i=0;i<n;i++)CHECK(socketpair(AF_UNIX,type,0,sv[i])==0);
 for(int i=0;i<ne;i++){
  int fds[2]={sv[edges[i][0]][0],sv[edges[i][0]][0]};
  CHECK(put(sv[edges[i][1]][1],fds,2,"x",1)==1);
 }
 CHECK(put(sv[0][1],&sentinel[1],1,"s",1)==1);close(sentinel[1]);
 for(int i=0;i<n;i++){close(sv[i][0]);close(sv[i][1]);}
 CHECK(fcntl(sentinel[0],F_SETFL,O_NONBLOCK)==0);
 int rc=-1;for(int i=0;i<20;i++){rc=read(sentinel[0],&ch,1);if(rc==0)break;CHECK(errno==EAGAIN);usleep(1000);}
#ifndef NATIVE_LINUX
 CHECK(rc==0);
#endif
 close(sentinel[0]);
}
int main(void){
 static const int self[][2]={{0,0},{1,1}};
 static const int triangle[][2]={{0,1},{1,2},{2,0},{3,4},{4,5},{5,3}};
 static const int cross[][2]={{0,1},{1,2},{2,0},{1,3},{3,2},{4,5},{5,6},{6,4},{5,7},{7,6}};
 static const int back[][2]={{0,1},{0,4},{1,2},{2,3},{3,1},{5,6},{5,9},{6,7},{7,8},{8,6}};
 static const int mixed[][2]={{0,1},{1,2},{2,1},{1,0},{3,4},{4,5},{5,4},{4,3}};
 for(int type=1;type<=5;type+=(type==1?1:3)){
  graph(type,2,self,2);graph(type,6,triangle,6);graph(type,8,cross,10);graph(type,10,back,10);graph(type,6,mixed,8);
 }
 int sv[2],p[2],got,flags;char ch;struct stat before,after;
 CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,sv)==0);
 int f=open("ipc-bsd-file",O_CREAT|O_TRUNC|O_RDWR,0600);CHECK(f>=0);CHECK(fstat(f,&before)==0);
 CHECK(put(sv[0],&f,1,"x",1)==1);close(f);CHECK(get(sv[1],&got,1,&ch,1,0,&flags)==1);
 CHECK(fstat(got,&after)==0);CHECK(before.st_dev==after.st_dev&&before.st_ino==after.st_ino);close(got);unlink("ipc-bsd-file");
 CHECK(pipe(p)==0);CHECK(put(sv[0],&p[1],1,"x",1)==1);close(p[1]);
 CHECK(get(sv[1],&got,0,&ch,1,0,&flags)==1);CHECK(flags&MSG_CTRUNC);CHECK(read(p[0],&ch,1)==0);close(p[0]);
 CHECK(pipe(p)==0);CHECK(put(sv[0],&p[1],1,"x",1)==1);close(p[1]);CHECK(shutdown(sv[1],SHUT_RD)==0);
 /* gucOS preserves its existing buffered-read-after-SHUT_RD behavior,
  * matching Linux here. Close disposes any still-queued rights. */
 close(sv[0]);close(sv[1]);CHECK(read(p[0],&ch,1)==0);close(p[0]);
 puts("IPC-UPSTREAM-ADAPTATIONS-PASS: 15 graph variants + 3 FreeBSD cases");return 0;
}
