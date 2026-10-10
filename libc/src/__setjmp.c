
__exception __LongJump(int, int);
/* Calls are continuation intrinsics in the memory-frame backend. Keeping
   a non-inline definition gives ordinary function pointers a typed slot. */
__attribute__((noinline)) int setjmp(int *env) { return 0; }
void longjmp(int *env, int val) {
  __throw __LongJump(env[0], val ? val : 1);
}
  