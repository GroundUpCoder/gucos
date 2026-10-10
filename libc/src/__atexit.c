
static void (*__atexit_funcs[32])(void);
static int __atexit_count = 0;

int atexit(void (*func)(void)) {
  if (__atexit_count >= 32) return -1;
  __atexit_funcs[__atexit_count++] = func;
  return 0;
}

void __run_atexits(void) {
  while (__atexit_count > 0)
    __atexit_funcs[--__atexit_count]();
}
__export __run_atexits = __run_atexits;
  