
#pragma once
__require_source("__inet.c");
#include <netinet/in.h>
const char *inet_ntop(int family, const void *src, char *dst, socklen_t size);
int inet_pton(int family, const char *src, void *dst);
int inet_aton(const char *src, struct in_addr *dst);
  