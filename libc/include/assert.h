
__require_source("__assert.c");
void __assert_fail(const char *expr, const char *file, int line);
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))
#endif
#define static_assert _Static_assert
  