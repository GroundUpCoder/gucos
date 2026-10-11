/* Hold a kernel record lock across exec. Exit or process death releases it. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv){
 if(argc<4){fputs("usage: guc-lock LOCK COMMAND ARG...\n",stderr);return 2;}
 int fd=open(argv[1],O_RDWR|O_CREAT,0600);
 if(fd<0){perror("guc-lock open");return 1;}
 struct flock lock;memset(&lock,0,sizeof lock);lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
 if(fcntl(fd,F_SETLK,&lock)<0){fprintf(stderr,"gucman: another package command is active: %s\n",strerror(errno));close(fd);return 1;}
 execv(argv[2],argv+2);perror("guc-lock exec");close(fd);return 126;
}
