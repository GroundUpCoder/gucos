
#pragma once
/* __SDL.c's private window record + registry, shared with the popup TU
   (__SDL_popup.c, docs/archive/0256). NOT for app code — the public headers keep
   SDL_Window opaque. 'handle' is a 1-based index into the host's window
   table, reused as the SDL window ID; the registry array lets
   __sdl_push_window_event re-derive a window's surface on RESIZED — popup
   windows slot into it directly (the register/unregister helpers stay
   static in __SDL.c: a cross-TU reference would defeat their single-use
   inlining and change every SDL binary's bytes). */
#include <SDL.h>

struct SDL_Window {
    int handle;
    SDL_Surface surface;
    int pixels_cap;      /* high-water byte size of surface.pixels (resize) */
    void *present_pixels; /* reusable RGBA transport buffer; surface stays BGR */
    int present_cap;
    int present_direct; /* 0 unknown, 1 host uploads the BGR surface itself, 2 it needs RGBA */
    bool relative_mouse; /* requested relative-mouse mode (docs/archive/0018) */
    SDL_WindowFlags flags;  /* create-time flags (#601 SDL_GetWindowFlags —
                               the static contract documented in SDL.h) */
    SDL_Renderer *renderer; /* the window's one renderer, or NULL (#497 —
                               SDL3 allows one per window; SDL_CreateRenderer
                               sets, SDL_DestroyRenderer clears) */
    int parent_handle;      /* owner window handle (#794 SDL_SetWindowParent),
                               0 = unowned */
    int close_reason;       /* GUC_CLOSE_REASON_* of the last close event */
};

#define __SDL_MAX_WINDOWS 32
extern SDL_Window *__sdl_window_registry[__SDL_MAX_WINDOWS];

  