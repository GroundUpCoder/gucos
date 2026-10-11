
#pragma once
#include <sys/socket.h>
#include <string.h>
struct sockaddr_un {
  sa_family_t sun_family;
  char sun_path[108];
};
#define SUN_LEN(su) ((socklen_t)(sizeof(sa_family_t) + strlen((su)->sun_path)))
  