#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#define C(x) do{if(!(x)){fprintf(stderr,"tty line %d: %s errno=%d\n",__LINE__,#x,errno);exit(1);}}while(0)
int main(void){
 int m,t,m2,t2,s[2]; C(openpty(&m,&t,0,0,0)==0);C(openpty(&m2,&t2,0,0,0)==0);
 struct winsize w={41,117,0,0},v={19,63,0,0},got;
 C(ioctl(m,TIOCSWINSZ,&w)==0);C(ioctl(m2,TIOCSWINSZ,&v)==0);
 C(ioctl(t,TIOCGWINSZ,&got)==0&&got.ws_row==41&&got.ws_col==117);
 C(ioctl(m2,TIOCGWINSZ,&got)==0&&got.ws_row==19&&got.ws_col==63);
 C(tcsetpgrp(t,getpgrp())==0);C(socketpair(AF_UNIX,SOCK_STREAM,0,s)==0);
 pid_t pid=fork();C(pid>=0);
 if(!pid){
  close(s[0]);close(m);close(t);close(m2);close(t2);C(setsid()>=0);
  signal(SIGTTIN,SIG_IGN); /* Incorrect job control becomes EIO, not a hang. */
  char control[CMSG_SPACE(sizeof(int))]={0},b;struct iovec i={&b,1};
  struct msghdr h={0};h.msg_iov=&i;h.msg_iovlen=1;h.msg_control=control;h.msg_controllen=sizeof control;
  C(recvmsg(s[1],&h,0)==1);struct cmsghdr *c=CMSG_FIRSTHDR(&h);C(c!=0);int fd;memcpy(&fd,CMSG_DATA(c),sizeof fd);
  C(ioctl(fd,TIOCGWINSZ,&got)==0&&got.ws_row==41&&got.ws_col==117);
  C(write(s[1],"R",1)==1);char input[2];C(read(fd,input,2)==2&&input[0]=='X'&&input[1]=='\n');
  close(fd);_exit(0);
 }
 close(s[1]);char control[CMSG_SPACE(sizeof(int))]={0},b='T';struct iovec i={&b,1};struct msghdr h={0};
 h.msg_iov=&i;h.msg_iovlen=1;h.msg_control=control;h.msg_controllen=sizeof control;
 struct cmsghdr *c=CMSG_FIRSTHDR(&h);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));memcpy(CMSG_DATA(c),&t,sizeof t);
 C(sendmsg(s[0],&h,0)==1);C(read(s[0],&b,1)==1&&b=='R');C(write(m,"X\n",2)==2);
 int status;C(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 /* This fixture assigned our own process group to the test PTY. Closing
    its final master signals that group; ignore HUP only during cleanup. */
 signal(SIGHUP,SIG_IGN);
 close(m);close(t);close(m2);close(t2);close(s[0]);puts("IPC-TERMINAL-PASS");return 0;
}
