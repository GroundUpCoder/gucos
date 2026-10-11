
#include <string.h>
#include <guc.h>

__externref __jss(const char *s) {
    return __jsstr2(s, (int)strlen(s));
}
  