
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
__import int __pty_control(int,int);
int posix_openpt(int flags) { return open("/dev/ptmx",flags); }
int grantpt(int fd) { return __pty_control(fd,2); }
int unlockpt(int fd) { return __pty_control(fd,3); }
int ptsname_r(int fd,char *buf,size_t size) {
  int saved=errno,id=__pty_control(fd,1);
  if (id<0) { int e=errno;errno=saved;return e; }
  char name[32];snprintf(name,sizeof name,"/dev/pts/%d",id);
  if (!buf) return EINVAL;
  if (strlen(name)>=size) return ERANGE;
  strcpy(buf,name);return 0;
}
char *ptsname(int fd) {
  static char name[32];int e=ptsname_r(fd,name,sizeof name);
  if(e) { errno=e;return NULL; }return name;
}
