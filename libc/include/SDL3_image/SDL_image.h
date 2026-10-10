
#pragma once
#include <SDL3/SDL.h>
__require_source("__SDL_image.c");

SDL_Surface *IMG_Load(const char *file);
SDL_Texture *IMG_LoadTexture(SDL_Renderer *renderer, const char *file);
/* SDL_image shares SDL's per-thread error string. */
const char *IMG_GetError(void);
  