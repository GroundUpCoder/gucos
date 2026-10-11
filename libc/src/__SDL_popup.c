
#include <SDL_popup.h>
#include <__SDL_internal.h>
#include <stdlib.h>
#include <string.h>

/* Stock SDL3 popup windows + display bounds (docs/archive/0256) — its own TU (see
   SDL_popup.h) so these two imports never land in a non-popup binary's
   import table. */

/* Anchored popup window: parent_handle names the parent window, dx/dy the
   anchor offset in parent client coords; flags carry POPUP_MENU/TOOLTIP so
   the host maps the kernel grab. Returns a handle in the SAME space as
   __sdl_create_window (one per-handle table), 0 on failure. */
__import int __sdl_create_popup_window(int parent_handle, int dx, int dy, int w, int h, int flags);
/* Screen dims packed (w << 16) | h, 0 = no display authority. */
__import int __sdl_get_display_bounds(void);

/* An anchored child surface under the OS WM — pinned to the parent,
   chrome-free, never focused, dismissed via SDL_EVENT_WINDOW_CLOSE_REQUESTED
   when a POPUP_MENU grab is broken by an outside press. The returned
   SDL_Window is ordinary in every other way (GetWindowSurface /
   UpdateWindowSurface / SetWindowSize / DestroyWindow all work per-handle;
   registration below keeps RESIZED surface re-derivation working). */
SDL_Window *SDL_CreatePopupWindow(SDL_Window *parent, int offset_x, int offset_y,
                                  int w, int h, SDL_WindowFlags flags) {
    if (!parent) { SDL_SetError("SDL_CreatePopupWindow: parent is NULL"); return NULL; }
    if (!(flags & (SDL_WINDOW_POPUP_MENU | SDL_WINDOW_TOOLTIP))) {
        /* SDL3 contract: a popup must declare which kind it is. */
        SDL_SetError("SDL_CreatePopupWindow: flags must include SDL_WINDOW_POPUP_MENU or SDL_WINDOW_TOOLTIP");
        return NULL;
    }
    int handle = __sdl_create_popup_window(parent->handle, offset_x, offset_y, w, h, (int)flags);
    if (handle <= 0) { SDL_SetError("SDL_CreatePopupWindow: this runtime cannot create popup windows"); return NULL; }
    int pitch = w * 4;
    SDL_Window *win = (SDL_Window *)malloc(sizeof(SDL_Window));
    if (!win) { SDL_SetError("Out of memory"); return NULL; }
    win->handle = handle;
    win->surface.flags = 0;
    /* Ordinary windows are opaque: integer pixel 0 must mean black, not
       transparent black. Keep per-pixel alpha only for transparent windows. */
    win->surface.format = (flags & SDL_WINDOW_TRANSPARENT)
        ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_BGRX32;
    win->surface.w = w;
    win->surface.h = h;
    win->surface.pitch = pitch;
    win->surface.refcount = 1;
    win->surface.reserved = NULL;
    win->surface.pixels = malloc(pitch * h);
    if (!win->surface.pixels) { free(win); SDL_SetError("Out of memory"); return NULL; }
    memset(win->surface.pixels, 0, pitch * h);
    win->pixels_cap = pitch * h;
    win->present_pixels = NULL;
    win->present_cap = 0;
    win->present_direct = 0;
    win->relative_mouse = 0;
    /* A popup is created chrome-free by construction, so BORDERLESS is part
       of its honest create-time flags (#601 SDL_GetWindowFlags). */
    win->flags = flags | SDL_WINDOW_BORDERLESS;
    win->renderer = NULL;
    win->parent_handle = parent->handle;   /* the anchor, never re-parentable */
    win->close_reason = 0;
    /* Slot into __SDL.c's registry directly (see __SDL_internal.h): RESIZED
       re-derivation and per-window close routing then cover popups exactly
       like top-levels. Past the cap the window still works, it just never
       re-derives on resize — the __SDL.c rule, verbatim. */
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++) {
        if (!__sdl_window_registry[i]) { __sdl_window_registry[i] = win; break; }
    }
    return win;
}

/* The OS screen as ONE synthetic display at origin 0,0 (displayID ignored).
   Reads the kernel-published dims with zero RPCs; fails loud where no
   window system exists. */
bool SDL_GetDisplayBounds(Uint32 displayID, SDL_Rect *rect) {
    (void)displayID;
    if (!rect) return SDL_SetError("SDL_GetDisplayBounds: rect is NULL");
    int packed = __sdl_get_display_bounds();
    if (packed <= 0)
        return SDL_SetError("SDL_GetDisplayBounds: no display authority in this runtime");
    rect->x = 0;
    rect->y = 0;
    rect->w = (packed >> 16) & 0xFFFF;
    rect->h = packed & 0xFFFF;
    return 1;
}

  