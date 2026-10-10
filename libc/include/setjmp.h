
#pragma once
__require_source("__setjmp.c");
/* The buffer points into continuation storage owned by the saving frame. */
typedef int jmp_buf[1];
__attribute__((noinline)) int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
/* Keep continuation capture in the caller, after evaluating both arguments
   exactly once. The mask is separate from the frame-owned continuation. */
typedef struct { jmp_buf __continuation; int __save_mask; unsigned long long __mask; } sigjmp_buf[1];
__require_source_if("__sigjmp_prepare", "__sigsetjmp.c");
__require_source_if("siglongjmp", "__sigsetjmp.c");
int *__sigjmp_prepare(sigjmp_buf env, int savemask);
void siglongjmp(sigjmp_buf env, int val);
#define sigsetjmp(env, savemask) setjmp(__sigjmp_prepare((env), (savemask)))
