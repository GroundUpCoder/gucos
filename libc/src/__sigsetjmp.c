
#include <setjmp.h>
#include <signal.h>
int *__sigjmp_prepare(sigjmp_buf env, int savemask) {
  env[0].__save_mask = savemask != 0;
  if (savemask) sigprocmask(SIG_SETMASK, 0, &env[0].__mask);
  return env[0].__continuation;
}
void siglongjmp(sigjmp_buf env, int val) {
  if (env[0].__save_mask) sigprocmask(SIG_SETMASK, &env[0].__mask, 0);
  longjmp(env[0].__continuation, val);
}
  