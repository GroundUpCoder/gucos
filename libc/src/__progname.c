
#include <stdlib.h>
static const char *__program_name = "";
const char *getprogname(void) { return __program_name; }
void setprogname(const char *name) {
  __program_name = name ? name : "";
  for (const char *p = __program_name; *p; p++) if (*p == '/') __program_name = p + 1;
}
__export __set_program_name = setprogname;
