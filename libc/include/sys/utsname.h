
#pragma once
#include <string.h>
struct utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
};
/* Fixed identity for the wasm runtime (no real kernel to query). */
static inline int uname(struct utsname *buf) {
  if (buf == 0) return -1;
  strcpy(buf->sysname,  "wasm");
  strcpy(buf->nodename, "localhost");
  strcpy(buf->release,  "1.0.0");
  strcpy(buf->version,  "c-compiler");
  strcpy(buf->machine,  "wasm32");
  return 0;
}
  