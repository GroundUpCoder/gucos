
#include <sys/socket.h>
ssize_t sendmsg(int fd,const struct msghdr *m,int flags){
 return __sock_message(fd,(struct msghdr *)m,flags,1);
}
ssize_t recvmsg(int fd,struct msghdr *m,int flags){
 return __sock_message(fd,m,flags,0);
}
int socket(int domain, int type, int protocol) {
  int flags=type;
  type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
  if (domain != AF_UNIX) { errno = EAFNOSUPPORT; return -1; }
  if ((type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) || protocol != 0) { errno = EPROTONOSUPPORT; return -1; }
  int fd=__sock_socket(domain,type,protocol);
  if(fd>=0){if(flags&SOCK_CLOEXEC)fcntl(fd,F_SETFD,FD_CLOEXEC);if(flags&SOCK_NONBLOCK)fcntl(fd,F_SETFL,O_NONBLOCK);}
  return fd;
}
int socketpair(int domain, int type, int protocol, int sv[2]) {
  int flags=type;
  type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
  if (domain != AF_UNIX) { errno = EAFNOSUPPORT; return -1; }
  if ((type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) || protocol != 0) { errno = EPROTONOSUPPORT; return -1; }
  int result=type==SOCK_STREAM?__sock_pair(sv):__sock_pair_type(sv,type);
  if(!result)for(int i=0;i<2;i++){if(flags&SOCK_CLOEXEC)fcntl(sv[i],F_SETFD,FD_CLOEXEC);if(flags&SOCK_NONBLOCK)fcntl(sv[i],F_SETFL,O_NONBLOCK);}
  return result;
}
int bind(int fd, const struct sockaddr *addr, socklen_t len) {
  char __p[109];
  if (__sockaddr_un_path(addr, len, __p) < 0) return -1;
  return __sock_bind(fd, __p);
}
int connect(int fd, const struct sockaddr *addr, socklen_t len) {
  char __p[109];
  if (__sockaddr_un_path(addr, len, __p) < 0) return -1;
  return __sock_connect(fd, __p);
}
int listen(int fd, int backlog) { return __sock_listen(fd, backlog); }
int accept(int fd, struct sockaddr *addr, socklen_t *len) {
  int nfd = __sock_accept(fd);
  if (nfd >= 0 && addr && getpeername(nfd, addr, len) < 0) {
    int saved = errno; close(nfd); errno = saved; return -1;
  }
  return nfd;
}
int shutdown(int fd, int how) { return __sock_shutdown(fd, how); }
long send(int fd, const void *buf, size_t n, int flags) {
  return __sock_send(fd, buf, n, flags);
}
long recv(int fd, void *buf, size_t n, int flags) {
  return __sock_recv(fd, buf, n, flags);
}
long sendto(int fd, const void *buf, size_t n, int flags,
                          const struct sockaddr *addr, socklen_t len) {
  struct iovec iov={(void *)buf,n};struct msghdr m={(void *)addr,len,&iov,1,0,0,0};
  return sendmsg(fd,&m,flags);
}
long recvfrom(int fd, void *buf, size_t n, int flags,
                            struct sockaddr *addr, socklen_t *len) {
  if(addr&&!len){errno=EFAULT;return -1;}
  struct iovec iov={buf,n};struct msghdr m={addr,len?*len:0,&iov,1,0,0,0};
  long result=recvmsg(fd,&m,flags);if(result>=0&&len)*len=m.msg_namelen;return result;
}
int setsockopt(int fd, int level, int opt, const void *v, socklen_t l) {
  return __sock_option(fd, level, opt, 1);
}
int getsockopt(int fd, int level, int opt, void *v, socklen_t *l) {
  if(level==SOL_SOCKET && opt==SO_PEERCRED){
    struct ucred credentials;
    if(__sock_peercred(fd,&credentials)<0)return -1;
    if(!l||(!v&&*l)){errno=EFAULT;return -1;}
    socklen_t n=*l<sizeof(credentials)?*l:sizeof(credentials);
    if(n)memcpy(v,&credentials,n);*l=n;return 0;
  }
  int value = __sock_option(fd, level, opt, 0);
  if (value < 0) return -1;
  if (!l || (!v && *l)) { errno = EFAULT; return -1; }
  socklen_t n = *l < sizeof(value) ? *l : sizeof(value);
  if (n) memcpy(v, &value, n);
  *l = n;
  return 0;
}
int getsockname(int fd, struct sockaddr *addr, socklen_t *len) {
  return __socket_name(fd, addr, len, 0);
}
int getpeername(int fd, struct sockaddr *addr, socklen_t *len) {
  return __socket_name(fd, addr, len, 1);
}
