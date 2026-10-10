
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
__import void __sdl_set_animation_frame_func(void (*callback)(void));
__import void __emscripten_async_call(void (*func)(void *), void *arg, int millis);
__import float __emscripten_random(void);

void emscripten_set_main_loop(void (*func)(void), int fps, int simulate_infinite_loop) {
  if (fps != 0) {
    printf("emscripten_set_main_loop: unsupported fps=%d (only 0 is supported)\n", fps);
    exit(1);
  }
  (void)simulate_infinite_loop;
  __sdl_set_animation_frame_func(func);
}

void emscripten_async_call(void (*func)(void *), void *arg, int millis) {
  __emscripten_async_call(func, arg, millis);
}

float emscripten_random(void) {
  return __emscripten_random();
}
  