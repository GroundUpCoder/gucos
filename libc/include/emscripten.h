
#pragma once
__require_source("__emscripten.c");
#define EMSCRIPTEN_KEEPALIVE
void emscripten_set_main_loop(void (*func)(void), int fps, int simulate_infinite_loop);
void emscripten_async_call(void (*func)(void *), void *arg, int millis);
float emscripten_random(void);

// Async HTTP fetch callback typedefs. Used as parameter types of
// emscripten_async_wget3_data() / emscripten_async_wget2_data() in code that
// downloads remote resources at runtime (e.g., tinyemu's network-backed
// disk-image feature). The function itself is not provided here — the
// expectation is that calls into the wget pipeline are dead-code-eliminated
// in builds that don't enable network features.
typedef void (*em_async_wget2_data_onload_func)(unsigned int handle, void *arg, void *data, unsigned int size);
typedef void (*em_async_wget2_data_onerror_func)(unsigned int handle, void *arg, int http_status, const char *status_text);
typedef void (*em_async_wget2_data_onprogress_func)(unsigned int handle, void *arg, int loaded, int total);
  