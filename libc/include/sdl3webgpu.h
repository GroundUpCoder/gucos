
#pragma once
/* SDL3 + WebGPU bridge (the eliemichel/sdl3webgpu precedent). Get a WGPUSurface
   for an SDL window. On this web backend the SDL window and the WebGPU surface
   are the SAME canvas, so this just creates the surface from it — but the call
   shape matches native sdl3webgpu so its examples port unchanged. Requires the
   SDL window NOT to have presented via SDL_UpdateWindowSurface (a canvas yields
   one context type for its lifetime): use either the software path OR WebGPU. */
#include <SDL.h>
#include <webgpu.h>
__require_source("__sdl3webgpu.c");

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window *window);

  