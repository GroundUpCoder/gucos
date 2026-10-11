#include <termios.h>
#include <pty.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#define C(x) do { if(!(x)){fprintf(stderr,"echo line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void exact(int fd,const char *s,size_t n){
  char b[128];C(n<=sizeof b);size_t pos=0;
  while(pos<n){fd_set f;FD_ZERO(&f);FD_SET(fd,&f);struct timeval tv={5,0};
    C(select(fd+1,&f,0,0,&tv)==1);ssize_t k=read(fd,b+pos,n-pos);C(k>0);pos+=k;}
  C(memcmp(b,s,n)==0);
}
#define EXPECT(fd,s) exact(fd,s,sizeof(s)-1)
int main(int argc,char **argv){
  (void)argv;
  if(argc>1){
    struct termios old,t;C(tcgetattr(0,&old)==0);t=old;
    t.c_lflag|=ICANON|ECHO|ECHOE|ECHOK|ECHOCTL;
    C(tcsetattr(0,TCSAFLUSH,&t)==0);
    puts("ECHO-KEYBOARD-READY");fflush(stdout);
    EXPECT(0,"ab\033[D\001\005Z\n");
    C(tcsetattr(0,TCSANOW,&old)==0);
    puts("ECHO-KEYBOARD-PASS");return 0;
  }
  int m,s;C(openpty(&m,&s,0,0,0)==0);struct termios t;
  C(tcgetattr(s,&t)==0);C(t.c_lflag&ECHOCTL);
  C(write(m,"a\033[D\001\005\n",7)==7);
  EXPECT(m,"a^[[D^A^E\r\n");EXPECT(s,"a\033[D\001\005\n");
  C(write(m,"\033\177Z\n",4)==4);
  EXPECT(m,"^[\b \b\b \bZ\r\n");EXPECT(s,"Z\n");
  t.c_lflag&=~(ICANON|ISIG);C(tcsetattr(s,TCSANOW,&t)==0);
  C(write(m,"\001\033\177\t\n",5)==5);
  EXPECT(m,"^A^[^?\t\r\n");EXPECT(s,"\001\033\177\t\n");
  t.c_lflag&=~ECHOCTL;C(tcsetattr(s,TCSANOW,&t)==0);
  C(write(m,"\001\033",2)==2);EXPECT(m,"\001\033");EXPECT(s,"\001\033");
  cfmakeraw(&t);C(tcsetattr(s,TCSANOW,&t)==0);
  C(write(m,"\033[D\001",4)==4);EXPECT(s,"\033[D\001");
  fd_set f;FD_ZERO(&f);FD_SET(m,&f);struct timeval tv={0,10000};
  C(select(m+1,&f,0,0,&tv)==0);
  close(m);close(s);puts("PTY-ECHO-PASS");return 0;
}
