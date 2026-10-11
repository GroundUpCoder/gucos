
#include <__thread.h>
int *__errno_location(void) { return (int *)(__builtin(thread_record) + __THREAD_ERRNO); }
void __errno_set(int e) { *__errno_location() = e; }
__export __errno_set = __errno_set;
  