
#pragma once
#include <sys/socket.h>
#include <stdint.h>
#include <byteswap.h>
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };
struct in6_addr { unsigned char s6_addr[16]; };
struct sockaddr_in { sa_family_t sin_family; in_port_t sin_port; struct in_addr sin_addr; unsigned char sin_zero[8]; };
struct sockaddr_in6 { sa_family_t sin6_family; in_port_t sin6_port; uint32_t sin6_flowinfo; struct in6_addr sin6_addr; uint32_t sin6_scope_id; };
struct ip_mreq { struct in_addr imr_multiaddr,imr_interface; };
struct ip_mreq_source { struct in_addr imr_multiaddr,imr_interface,imr_sourceaddr; };
struct ipv6_mreq { struct in6_addr ipv6mr_multiaddr; unsigned ipv6mr_interface; };
struct group_source_req { unsigned gsr_interface; struct sockaddr_storage gsr_group,gsr_source; };
#define IP_TTL 2
#define IP_MULTICAST_IF 32
#define IP_MULTICAST_TTL 33
#define IP_MULTICAST_LOOP 34
#define IP_ADD_MEMBERSHIP 35
#define IP_DROP_MEMBERSHIP 36
#define IP_ADD_SOURCE_MEMBERSHIP 39
#define IP_DROP_SOURCE_MEMBERSHIP 40
#define MCAST_JOIN_SOURCE_GROUP 46
#define MCAST_LEAVE_SOURCE_GROUP 47
#define IPV6_UNICAST_HOPS 16
#define IPV6_MULTICAST_IF 17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_ADD_MEMBERSHIP 20
#define IPV6_DROP_MEMBERSHIP 21
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP

#define IN_MULTICAST(a) (((a)&0xf0000000U)==0xe0000000U)
#define INADDR_ANY 0U
#define INADDR_LOOPBACK 0x7f000001U
#define INADDR_BROADCAST 0xffffffffU
#define INADDR_NONE 0xffffffffU
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPV6_V6ONLY 26
#define IN6ADDR_ANY_INIT {{0}}
#define IN6ADDR_LOOPBACK_INIT {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}}
static const struct in6_addr in6addr_any=IN6ADDR_ANY_INIT;
static const struct in6_addr in6addr_loopback=IN6ADDR_LOOPBACK_INIT;
#define IN6_IS_ADDR_UNSPECIFIED(a) (memcmp((a), &in6addr_any, 16)==0)
#define IN6_IS_ADDR_LOOPBACK(a) (memcmp((a), &in6addr_loopback, 16)==0)
#define IN6_IS_ADDR_V4MAPPED(a) (memcmp((a), "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12)==0)
#define IN6_IS_ADDR_LINKLOCAL(a) ((a)->s6_addr[0]==0xfe&&((a)->s6_addr[1]&0xc0)==0x80)
#define IN6_IS_ADDR_SITELOCAL(a) ((a)->s6_addr[0]==0xfe&&((a)->s6_addr[1]&0xc0)==0xc0)
#define IN6_IS_ADDR_V4COMPAT(a) (memcmp((a), &in6addr_any, 12)==0&&!IN6_IS_ADDR_UNSPECIFIED(a)&&!IN6_IS_ADDR_LOOPBACK(a))
#define IN6_IS_ADDR_MC_NODELOCAL(a) (IN6_IS_ADDR_MULTICAST(a)&&((a)->s6_addr[1]&15)==1)
#define IN6_IS_ADDR_MC_LINKLOCAL(a) (IN6_IS_ADDR_MULTICAST(a)&&((a)->s6_addr[1]&15)==2)
#define IN6_IS_ADDR_MC_SITELOCAL(a) (IN6_IS_ADDR_MULTICAST(a)&&((a)->s6_addr[1]&15)==5)
#define IN6_IS_ADDR_MC_ORGLOCAL(a) (IN6_IS_ADDR_MULTICAST(a)&&((a)->s6_addr[1]&15)==8)
#define IN6_IS_ADDR_MC_GLOBAL(a) (IN6_IS_ADDR_MULTICAST(a)&&((a)->s6_addr[1]&15)==14)
#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0]==0xff)
static inline uint16_t htons(uint16_t x){return bswap_16(x);}
static inline uint16_t ntohs(uint16_t x){return bswap_16(x);}
static inline uint32_t htonl(uint32_t x){return bswap_32(x);}
static inline uint32_t ntohl(uint32_t x){return bswap_32(x);}
  