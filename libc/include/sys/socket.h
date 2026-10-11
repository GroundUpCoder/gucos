
#pragma once
__require_source("__socket_api.c");
/* AF_UNIX stream, datagram and sequenced-packet sockets with SCM_RIGHTS.
   Pathname addresses only; abstract addresses and writable socket options
   remain unsupported. Packet payloads are bounded to 60000 bytes. */
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef unsigned socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; char __ss_pad[126]; };

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_LOCAL  AF_UNIX
#define AF_INET   2
#define AF_INET6  10
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_LOCAL  AF_LOCAL
#define PF_INET   AF_INET

#define SOMAXCONN 128
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_SEQPACKET 5
#define SOCK_CLOEXEC  0x80000  /* == O_CLOEXEC */
#define SOCK_NONBLOCK 0x800    /* == O_NONBLOCK */

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

#define MSG_CMSG_CLOEXEC 0x40000000
#define MSG_PEEK     2
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000

#define SOL_SOCKET   1
#define SO_REUSEADDR 2
#include <sys/uio.h>
struct linger { int l_onoff, l_linger; };
struct msghdr { void *msg_name; socklen_t msg_namelen; struct iovec *msg_iov; size_t msg_iovlen; void *msg_control; size_t msg_controllen; int msg_flags; };
struct cmsghdr { size_t cmsg_len; int cmsg_level, cmsg_type; };
#define SCM_RIGHTS 1
#define MSG_TRUNC 0x20
#define MSG_CTRUNC 8
#define SO_TYPE 3
#define SO_BROADCAST 6
#define SO_LINGER 13
#define SO_REUSEPORT 15
#define CMSG_ALIGN(n) (((n)+sizeof(size_t)-1)&~(sizeof(size_t)-1))
#define CMSG_SPACE(n) (CMSG_ALIGN(sizeof(struct cmsghdr))+CMSG_ALIGN(n))
#define CMSG_LEN(n) (CMSG_ALIGN(sizeof(struct cmsghdr))+(n))
#define CMSG_DATA(c) ((unsigned char *)(c)+CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(m) ((m)->msg_controllen>=sizeof(struct cmsghdr)?(struct cmsghdr *)(m)->msg_control:0)
static inline struct cmsghdr *CMSG_NXTHDR(const struct msghdr *m,const struct cmsghdr *c){
 if(c->cmsg_len<sizeof(*c))return 0;
 char *p=(char *)c+CMSG_ALIGN(c->cmsg_len),*end=(char *)m->msg_control+m->msg_controllen;
 return p+sizeof(*c)<=end?(struct cmsghdr *)p:0;
}
/* One bounded gather/scatter operation, preserving short stream transfers. */
#include <stdlib.h>
__import long __sock_send_control(int fd,const void *buf,size_t n,int flags,const int *fds,int nfds,const char *path);
__import long __sock_recv_control(int fd,void *buf,size_t n,int flags,int *fds,int cap,int *info,char *name);
__import int __sock_option(int fd,int level,int option,int set);
static inline int __sockaddr_un_path(const struct sockaddr *addr,socklen_t len,char *out);
static inline ssize_t __sock_message(int fd,struct msghdr *m,int flags,int sending){
 if(!m || (m->msg_iovlen && !m->msg_iov)){errno=EFAULT;return -1;}
 if(m->msg_controllen && !m->msg_control){errno=EFAULT;return -1;}
 if(m->msg_iovlen>1024){errno=EMSGSIZE;return -1;}
 char path[109]={0};
 if(sending && m->msg_name && __sockaddr_un_path(m->msg_name,m->msg_namelen,path)<0)return -1;
 int fds[253],nfds=0,info[2]={0,0};
 if(sending){
  size_t at=0;
  while(m->msg_controllen-at>=sizeof(struct cmsghdr)){
   struct cmsghdr c;memcpy(&c,(char *)m->msg_control+at,sizeof(c));
   if(c.cmsg_len<CMSG_LEN(0)||c.cmsg_len>m->msg_controllen-at){errno=EINVAL;return -1;}
   if(c.cmsg_level!=SOL_SOCKET||c.cmsg_type!=SCM_RIGHTS){errno=EOPNOTSUPP;return -1;}
   size_t bytes=c.cmsg_len-CMSG_LEN(0);
   if(bytes%sizeof(int)||bytes/sizeof(int)>(size_t)(253-nfds)){errno=EINVAL;return -1;}
   memcpy(fds+nfds,(char *)m->msg_control+at+CMSG_LEN(0),bytes);nfds+=bytes/sizeof(int);
   size_t step=CMSG_ALIGN(c.cmsg_len);if(step>m->msg_controllen-at)break;at+=step;
  }
 }else if(m->msg_controllen>=CMSG_LEN(sizeof(int))){
  size_t cap=(m->msg_controllen-CMSG_LEN(0))/sizeof(int);nfds=cap>253?253:cap;
 }
 size_t total=0;
 for(size_t i=0;i<m->msg_iovlen;i++){
  size_t n=m->msg_iov[i].iov_len;
  if(n>0x7fffffff-total){errno=EMSGSIZE;return -1;}
  if(n && !m->msg_iov[i].iov_base){errno=EFAULT;return -1;}total+=n;
 }
 if(sending && total>60000){
  int type=__sock_option(fd,SOL_SOCKET,SO_TYPE,0);if(type<0)return -1;
  if(type!=SOCK_STREAM){errno=EMSGSIZE;return -1;}
 }
 if(total>60000)total=60000;
 unsigned char *buf=malloc(total?total:1);if(!buf){errno=ENOMEM;return -1;}
 size_t offset=0;
 if(sending)for(size_t i=0;i<m->msg_iovlen && offset<total;i++){
  size_t n=m->msg_iov[i].iov_len;if(n>total-offset)n=total-offset;
  memcpy(buf+offset,m->msg_iov[i].iov_base,n);offset+=n;
 }
 ssize_t result=sending?__sock_send_control(fd,buf,total,flags,fds,nfds,path):__sock_recv_control(fd,buf,total,flags,fds,nfds,info,path);
 if(!sending && result>=0){
  offset=0;size_t copied=(size_t)result<total?(size_t)result:total;
  for(size_t i=0;i<m->msg_iovlen && offset<copied;i++){
   size_t n=m->msg_iov[i].iov_len;if(n>copied-offset)n=copied-offset;
   memcpy(m->msg_iov[i].iov_base,buf+offset,n);offset+=n;
  }
  size_t controlcap=m->msg_controllen;m->msg_controllen=0;m->msg_flags=info[0];
  if(m->msg_name){
   struct {sa_family_t family;char path[108];} name={AF_UNIX,{0}};
   size_t len=strlen(path);memcpy(name.path,path,len);
   socklen_t actual=sizeof(sa_family_t)+(len?(len<108?len+1:len):0);
   memcpy(m->msg_name,&name,m->msg_namelen<actual?m->msg_namelen:actual);m->msg_namelen=actual;
  }else m->msg_namelen=0;
  if(info[1]){
   struct cmsghdr c={CMSG_LEN(info[1]*sizeof(int)),SOL_SOCKET,SCM_RIGHTS};
   memcpy(m->msg_control,&c,sizeof(c));memcpy((char *)m->msg_control+CMSG_LEN(0),fds,info[1]*sizeof(int));
   size_t used=CMSG_SPACE(info[1]*sizeof(int));m->msg_controllen=used<controlcap?used:controlcap;
  }
 }
 free(buf);return result;
}
ssize_t sendmsg(int fd,const struct msghdr *m,int flags);
ssize_t recvmsg(int fd,struct msghdr *m,int flags);

#define SO_ACCEPTCONN 30
#define SO_PEERCRED 17
struct ucred { int pid; unsigned uid,gid; };
#define SO_ERROR     4
#define SO_SNDBUF    7
#define SO_RCVBUF    8
#define SO_KEEPALIVE 9

__import int __sock_socket(int domain, int type, int protocol);
__import int __sock_bind(int fd, const char *path);
__import int __sock_listen(int fd, int backlog);
__import int __sock_accept(int fd);
__import int __sock_connect(int fd, const char *path);
__import int __sock_pair(int sv[2]);
__import int __sock_pair_type(int sv[2],int type);
__import int __sock_shutdown(int fd, int how);
__import int __sock_name(int fd, int peer, char *path);
__import long __sock_send(int fd, const void *buf, size_t n, int flags);
__import long __sock_recv(int fd, void *buf, size_t n, int flags);
__import int __sock_option(int fd, int level, int option, int set);
__import int __sock_peercred(int fd, void *credentials);

/* Extract the NUL-terminated fs path from a sockaddr_un-shaped address
   (family at offset 0, path bytes right after — see <sys/un.h>). */
static inline int __sockaddr_un_path(const struct sockaddr *addr, socklen_t len,
                                     char *out /* [109] */) {
  if (!addr || len < (socklen_t)sizeof(sa_family_t)) { errno = EINVAL; return -1; }
  if (addr->sa_family != AF_UNIX) { errno = EAFNOSUPPORT; return -1; }
  {
    const char *p = (const char *)addr + sizeof(sa_family_t);
    socklen_t max = len - (socklen_t)sizeof(sa_family_t);
    socklen_t n = 0;
    if (max > 108) { errno = EINVAL; return -1; }
    if (max && p[0] == 0) { errno = EOPNOTSUPP; return -1; }
    while (n < max && p[n]) n++;
    if (n == 0) { errno = EINVAL; return -1; }          /* unnamed */
    if (p[0] == 0) { errno = EOPNOTSUPP; return -1; }   /* abstract ns */
    memcpy(out, p, n);
    out[n] = 0;
  }
  return 0;
}

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int fd, const struct sockaddr *addr, socklen_t len);
int connect(int fd, const struct sockaddr *addr, socklen_t len);
int listen(int fd, int backlog);
int getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int accept(int fd, struct sockaddr *addr, socklen_t *len);
int shutdown(int fd, int how);
long send(int fd, const void *buf, size_t n, int flags);
long recv(int fd, void *buf, size_t n, int flags);
long sendto(int fd, const void *buf, size_t n, int flags,
                          const struct sockaddr *addr, socklen_t len);
long recvfrom(int fd, void *buf, size_t n, int flags,
                            struct sockaddr *addr, socklen_t *len);
/* Kernel owns socket state across dup/fork. Unsupported options fail. */
int setsockopt(int fd, int level, int opt, const void *v, socklen_t l);
int getsockopt(int fd, int level, int opt, void *v, socklen_t *l);
static inline int __socket_name(int fd, struct sockaddr *addr, socklen_t *len, int peer) {
  char path[109];
  int n = __sock_name(fd, peer, path);
  if (n < 0) return -1;
  if (!len || (!addr && *len)) { errno = EFAULT; return -1; }
  /* sockaddr_un contains up to 108 pathname bytes. */
  if (n > 108) { errno = ENAMETOOLONG; return -1; }
  struct { sa_family_t family; char path[108]; } result = { AF_UNIX, {0} };
  if (n) memcpy(result.path, path, n);
  socklen_t size = sizeof(sa_family_t) + (n ? (n < 108 ? n + 1 : n) : 0);
  socklen_t copied = *len < size ? *len : size;
  if (copied) memcpy(addr, &result, copied);
  *len = size;
  return 0;
}
int getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int getpeername(int fd, struct sockaddr *addr, socklen_t *len);

  