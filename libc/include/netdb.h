
#pragma once
#include <sys/socket.h>
__require_source("__netdb.c");
struct addrinfo { int ai_flags,ai_family,ai_socktype,ai_protocol; socklen_t ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
struct servent { char *s_name; char **s_aliases; int s_port; char *s_proto; };
#define AI_PASSIVE 1
#define AI_CANONNAME 2
#define AI_NUMERICHOST 4
#define AI_NUMERICSERV 1024
#define EAI_BADFLAGS -1
#define EAI_NONAME -2
#define EAI_AGAIN -3
#define EAI_FAIL -4
#define EAI_FAMILY -6
#define EAI_SOCKTYPE -7
#define EAI_SERVICE -8
#define EAI_MEMORY -10
#define EAI_SYSTEM -11
#define EAI_OVERFLOW -12
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NAMEREQD 8
#define NI_DGRAM 16
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
int getaddrinfo(const char *,const char *,const struct addrinfo *,struct addrinfo **);
void freeaddrinfo(struct addrinfo *);
int getnameinfo(const struct sockaddr *,socklen_t,char *,socklen_t,char *,socklen_t,int);
const char *gai_strerror(int);
struct servent *getservbyname(const char *,const char *);
struct servent *getservbyport(int,const char *);
struct servent *getservent(void);
void setservent(int);
void endservent(void);
  