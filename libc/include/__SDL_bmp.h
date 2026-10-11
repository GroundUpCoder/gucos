
#ifndef __SDL_BMP_H
#define __SDL_BMP_H
__require_source("__SDL_bmp.c");
SDL_Surface *SDL_LoadBMP_IO(SDL_IOStream *src,bool closeio);
SDL_Surface *SDL_LoadBMP(const char *path);
bool SDL_SaveBMP_IO(SDL_Surface *surface,SDL_IOStream *dst,bool closeio);
bool SDL_SaveBMP(SDL_Surface *surface,const char *path);
#endif
