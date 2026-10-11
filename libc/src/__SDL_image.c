
#include <SDL3_image/SDL_image.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>

const char *IMG_GetError(void) { return SDL_GetError(); }

/* Decode 'file' to a freshly-malloc'd RGBA32 SDL_Surface (caller owns it;
   free with SDL_DestroySurface). NULL + SDL error on any failure. */
SDL_Surface *IMG_Load(const char *file) {
    if (!file) { SDL_InvalidParamError("file"); return NULL; }

    png_image pi;
    memset(&pi, 0, sizeof pi);
    pi.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&pi, file)) {
        SDL_SetError("IMG_Load('%s'): %s", file, pi.message);
        return NULL;
    }
    pi.format = PNG_FORMAT_RGBA;   /* force 8-bit RGBA, matching SDL_PIXELFORMAT_RGBA32 */

    int w = (int)pi.width, h = (int)pi.height, pitch = w * 4;
    void *px = malloc(PNG_IMAGE_SIZE(pi));
    if (!px) {
        png_image_free(&pi);
        SDL_SetError("IMG_Load('%s'): out of memory", file);
        return NULL;
    }
    /* row_stride 0 → libpng packs tightly (w*4 for RGBA), == pitch. */
    if (!png_image_finish_read(&pi, NULL, px, 0, NULL)) {
        SDL_SetError("IMG_Load('%s'): %s", file, pi.message);
        free(px);
        png_image_free(&pi);
        return NULL;
    }

    SDL_Surface *s = (SDL_Surface *)calloc(1, sizeof *s);
    if (!s) {
        free(px);
        SDL_SetError("IMG_Load('%s'): out of memory", file);
        return NULL;
    }
    s->flags = IMG_SURFACE_OWNED;   /* SDL_DestroySurface reclaims px + s */
    s->format = SDL_PIXELFORMAT_RGBA32;
    s->w = w; s->h = h; s->pitch = pitch;
    s->pixels = px; s->refcount = 1;
    return s;
}

/* Decode straight to a renderer texture (IMG_Load + upload + free surface),
   as SDL3_image's IMG_LoadTexture does. */
SDL_Texture *IMG_LoadTexture(SDL_Renderer *renderer, const char *file) {
    SDL_Surface *s = IMG_Load(file);
    if (!s) return NULL;
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_DestroySurface(s);
    return t;
}
  