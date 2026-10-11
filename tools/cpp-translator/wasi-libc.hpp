// Explicit WASI-header -> wasm-posix runtime boundary. Included only with
// --wasi-libc; source-level adaptation precedes LLVM's ABI lowering.
#define __assert_fail __wasm_posix_assert
#define pthread_create __wasm_posix_pthread_create
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#define setlocale __wasm_posix_setlocale
#include <locale.h>
// The accessor and error numbers must agree with the destination libc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma push_macro("__require_source")
#define __require_source(...)
extern "C" {
#include "../../libc/include/errno.h"
}
#pragma pop_macro("__require_source")
#pragma clang diagnostic pop
extern "C" int *__errno_location(void);
extern "C" FILE *__wasm_posix_stdin(void);
extern "C" FILE *__wasm_posix_stdout(void);
extern "C" FILE *__wasm_posix_stderr(void);
#undef errno
#define errno (*__errno_location())
#undef stdin
#undef stdout
#undef stderr
#define stdin (__wasm_posix_stdin())
#define stdout (__wasm_posix_stdout())
#define stderr (__wasm_posix_stderr())
// WASI clock IDs are opaque pointers, not the destination's integer IDs.
#define clock_gettime __wasm_posix_clock_gettime
#define clock_getres __wasm_posix_clock_getres
#include <time.h>
#undef CLOCK_REALTIME
#undef CLOCK_MONOTONIC
#define CLOCK_REALTIME ((clockid_t)1)
#define CLOCK_MONOTONIC ((clockid_t)2)
