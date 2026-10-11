
#pragma once
/* Stock SDL3 popup windows + display bounds (docs/archive/0256, the menu-uniform
   architecture's kernel anchored-child primitive). A subsidiary header on
   the sdl3webgpu.h precedent: the API is bone-stock SDL3 (in upstream it
   sits in SDL_video.h), but gucOS links veneer TUs whole, so the popup
   implementation and its two host imports live in their own TU — a binary
   that never creates popups keeps a byte-identical import table. */
#include <SDL.h>
__require_source("__SDL_popup.c");

/* A borderless child window anchored to the parent at (offset_x, offset_y)
   in parent client coordinates. Under the OS WM this is a kernel anchored
   child surface: moved/hidden/raised/destroyed/scaled with its parent
   (arbitrary nesting depth — a popup may parent another popup), never
   focused, clamped into the screen. Flags must include SDL_WINDOW_POPUP_MENU
   (holds the kernel grab while it lives: a press outside the popup's window
   tree dismisses it via SDL_EVENT_WINDOW_CLOSE_REQUESTED and the press is
   consumed) or SDL_WINDOW_TOOLTIP (no grab). Standalone runtimes have no
   window system for popups and return NULL. */
SDL_Window *SDL_CreatePopupWindow(SDL_Window *parent, int offset_x, int offset_y,
                                  int w, int h, SDL_WindowFlags flags);

/* Display bounds: the OS screen dims, origin always 0,0; displayID is
   ignored (one synthetic display). False where no window system exists. */
bool SDL_GetDisplayBounds(Uint32 displayID, SDL_Rect *rect);

  