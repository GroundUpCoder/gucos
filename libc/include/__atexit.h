
#pragma once
__require_source("__atexit.c");
int atexit(void (*func)(void));
void __run_atexits(void);
  