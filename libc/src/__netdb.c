

#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
/* Numeric-only resolver profile. No fabricated DNS answers. */
void freeaddrinfo(struct addrinfo *p){while(p){struct addrinfo *n=p->ai_next;free(p->ai_addr);free(p->ai_canonname);free(p);p=n;}}
static int numeric_port(const char *s,unsigned *out){
 if(!s||!*s){*out=0;return 1;}unsigned n=0;
 for(;*s;s++){if(*s<'0'||*s>'9')return 0;n=n*10+(*s-'0');if(n>65535)return 0;}*out=n;return 1;
}
static _Thread_local FILE *__services_file;
static _Thread_local int __services_stayopen;
void endservent(void){if(__services_file)fclose(__services_file);__services_file=NULL;__services_stayopen=0;}
void setservent(int stayopen){if(__services_file)rewind(__services_file);else __services_file=fopen("/etc/services","r");__services_stayopen=stayopen;}
struct servent *getservent(void){
 static _Thread_local struct servent entry;
 static _Thread_local char line[1024],service[128],proto[32],aliases[768];
 static _Thread_local char *aliasv[64];
 if(!__services_file)__services_file=fopen("/etc/services","r");
 if(!__services_file)return NULL;
 FILE *f=__services_file;
 while(fgets(line,sizeof line,f)){
  char *comment=strchr(line,'#');if(comment)*comment=0;
  char *save=NULL,*key=strtok_r(line," \t\r\n",&save),*spec=strtok_r(NULL," \t\r\n",&save);
  if(!key||!spec)continue;char *slash=strchr(spec,'/');if(!slash)continue;*slash++=0;
  unsigned port;if(!numeric_port(spec,&port)||strlen(key)>=sizeof service||strlen(slash)>=sizeof proto)continue;
  int count=0;size_t used=0;char *alias;
  while((alias=strtok_r(NULL," \t\r\n",&save))){size_t n=strlen(alias)+1;if(count>=63||used+n>sizeof aliases)break;memcpy(aliases+used,alias,n);aliasv[count++]=aliases+used;used+=n;}
  aliasv[count]=NULL;strcpy(service,key);strcpy(proto,slash);
  entry.s_name=service;entry.s_aliases=aliasv;entry.s_port=htons(port);entry.s_proto=proto;return &entry;
 }
 return NULL;
}
static struct servent *__find_service(const char *name,int port,const char *protocol){
 setservent(__services_stayopen);
 struct servent *entry;
 while((entry=getservent())){
  if(protocol&&strcmp(protocol,entry->s_proto))continue;
  int match=name?!strcmp(name,entry->s_name):port==entry->s_port;
  if(name)for(char **alias=entry->s_aliases;*alias;alias++)if(!strcmp(name,*alias))match=1;
  if(match)break;
 }
 if(!__services_stayopen)endservent();
 return entry;
}
struct servent *getservbyname(const char *name,const char *protocol){return __find_service(name,-1,protocol);}
struct servent *getservbyport(int port,const char *protocol){return __find_service(NULL,port,protocol);}

int getaddrinfo(const char *node,const char *service,const struct addrinfo *h,struct addrinfo **out){
 if(!out){errno=EFAULT;return EAI_SYSTEM;}*out=NULL;
 int family=h?h->ai_family:AF_UNSPEC,flags=h?h->ai_flags:0,type=h?h->ai_socktype:0,protocol=h?h->ai_protocol:0;
 if(flags&~(AI_PASSIVE|AI_CANONNAME|AI_NUMERICHOST|AI_NUMERICSERV)){return EAI_BADFLAGS;}
 if(family!=AF_UNSPEC&&family!=AF_INET&&family!=AF_INET6)return EAI_FAMILY;
 if(type&&type!=SOCK_STREAM&&type!=SOCK_DGRAM)return EAI_SOCKTYPE;
 if(protocol&&protocol!=IPPROTO_TCP&&protocol!=IPPROTO_UDP)return EAI_SERVICE;
 if((type==SOCK_STREAM&&protocol==IPPROTO_UDP)||(type==SOCK_DGRAM&&protocol==IPPROTO_TCP))return EAI_SOCKTYPE;
 if(!node&&!service)return EAI_NONAME;
 unsigned port;
 if(!numeric_port(service,&port)){
  if(flags&AI_NUMERICSERV)return EAI_NONAME;
  struct servent *e=getservbyname(service,type==SOCK_DGRAM||protocol==IPPROTO_UDP?"udp":"tcp");
  if(!e)return EAI_SERVICE;port=ntohs(e->s_port);
  if(!type)type=!strcmp(e->s_proto,"udp")?SOCK_DGRAM:SOCK_STREAM;
 }
 unsigned char address[16];int fams[2],nf=0;unsigned scope=0;
 char text[INET6_ADDRSTRLEN];const char *numeric=node,*canon=node;
 if(node){const char *percent=strchr(node,'%');if(percent){size_t n=percent-node;if(n>=sizeof text)return EAI_NONAME;memcpy(text,node,n);text[n]=0;numeric=text;
   if(!percent[1])return EAI_NONAME;
   for(const char *p=percent+1;*p;p++){if(*p<'0'||*p>'9'||scope>429496729U||(scope==429496729U&&*p>'5'))return EAI_NONAME;scope=scope*10+(*p-'0');}
  }
  if((family==AF_UNSPEC||family==AF_INET)&&!percent&&inet_pton(AF_INET,numeric,address)==1)fams[nf++]=AF_INET;
  else if((family==AF_UNSPEC||family==AF_INET6)&&inet_pton(AF_INET6,numeric,address)==1)fams[nf++]=AF_INET6;
  else if(!percent&&!(flags&AI_NUMERICHOST)&&!strcasecmp(node,"localhost")){
   /* No resolver here; the loopback name is answered like an /etc/hosts entry. */
   if(family!=AF_INET){fams[nf++]=AF_INET6;}
   if(family!=AF_INET6){fams[nf++]=AF_INET;}
   node=NULL;flags &= ~AI_PASSIVE;
  }
  else{if((flags&AI_NUMERICHOST)||inet_pton(AF_INET,numeric,address)==1||inet_pton(AF_INET6,numeric,address)==1)return EAI_NONAME;errno=ENOSYS;return EAI_SYSTEM;}
 }else{if(family!=AF_INET)fams[nf++]=AF_INET6;if(family!=AF_INET6)fams[nf++]=AF_INET;}
 struct addrinfo **tail=out;
 for(int i=0;i<nf;i++)for(int t=SOCK_STREAM;t<=SOCK_DGRAM;t++){
  if((type&&type!=t)||(protocol&&protocol!=(t==SOCK_STREAM?IPPROTO_TCP:IPPROTO_UDP)))continue;
  struct addrinfo *a=calloc(1,sizeof *a);if(!a){freeaddrinfo(*out);*out=NULL;return EAI_MEMORY;}
  a->ai_family=fams[i];a->ai_socktype=t;a->ai_protocol=t==SOCK_STREAM?IPPROTO_TCP:IPPROTO_UDP;a->ai_flags=flags;
  a->ai_addrlen=fams[i]==AF_INET?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6);
  a->ai_addr=calloc(1,a->ai_addrlen);*tail=a;tail=&a->ai_next;
  if(!a->ai_addr){freeaddrinfo(*out);*out=NULL;return EAI_MEMORY;}
  if(fams[i]==AF_INET){struct sockaddr_in *v=(void *)a->ai_addr;v->sin_family=AF_INET;v->sin_port=htons(port);if(node)memcpy(&v->sin_addr,address,4);else v->sin_addr.s_addr=htonl(flags&AI_PASSIVE?INADDR_ANY:INADDR_LOOPBACK);}
  else{struct sockaddr_in6 *v=(void *)a->ai_addr;v->sin6_family=AF_INET6;v->sin6_port=htons(port);v->sin6_scope_id=scope;if(node)memcpy(&v->sin6_addr,address,16);else v->sin6_addr=flags&AI_PASSIVE?in6addr_any:in6addr_loopback;}
 }
 if(canon&&(flags&AI_CANONNAME)){(*out)->ai_canonname=strdup(canon);if(!(*out)->ai_canonname){freeaddrinfo(*out);*out=NULL;return EAI_MEMORY;}}
 return 0;
}
int getnameinfo(const struct sockaddr *sa,socklen_t len,char *host,socklen_t hostlen,char *service,socklen_t servlen,int flags){
 if(flags&~(NI_NUMERICHOST|NI_NUMERICSERV|NI_NAMEREQD|NI_DGRAM))return EAI_BADFLAGS;
 if(!sa||len<sizeof(sa_family_t))return EAI_FAMILY;
 const void *addr;unsigned port,scope=0;
 if(sa->sa_family==AF_INET&&len>=sizeof(struct sockaddr_in)){const struct sockaddr_in *a=(const void *)sa;addr=&a->sin_addr;port=ntohs(a->sin_port);}
 else if(sa->sa_family==AF_INET6&&len>=sizeof(struct sockaddr_in6)){const struct sockaddr_in6 *a=(const void *)sa;addr=&a->sin6_addr;port=ntohs(a->sin6_port);scope=a->sin6_scope_id;}
 else return EAI_FAMILY;
 if(host&&hostlen){if((flags&NI_NAMEREQD)&&!(flags&NI_NUMERICHOST))return EAI_NONAME;char text[64];if(!inet_ntop(sa->sa_family,addr,text,sizeof text))return EAI_FAIL;if(scope){size_t n=strlen(text);snprintf(text+n,sizeof text-n,"%%%u",scope);}size_t n=strlen(text)+1;if(n>hostlen)return EAI_OVERFLOW;memcpy(host,text,n);}
 if(service&&servlen){char text[6];snprintf(text,sizeof text,"%u",port);size_t n=strlen(text)+1;if(n>servlen)return EAI_OVERFLOW;memcpy(service,text,n);}
 return 0;
}
const char *gai_strerror(int e){switch(e){case 0:return "Success";case EAI_BADFLAGS:return "Unsupported resolver flags";case EAI_NONAME:return "Name or service not known";case EAI_FAMILY:return "Unsupported address family";case EAI_SOCKTYPE:return "Unsupported socket type";case EAI_SERVICE:return "Unsupported service";case EAI_MEMORY:return "Out of memory";case EAI_SYSTEM:return "System error";case EAI_OVERFLOW:return "Buffer too small";default:return "Address resolution failed";}}

  