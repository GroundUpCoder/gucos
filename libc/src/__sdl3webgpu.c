
#include <sdl3webgpu.h>

__import int __wgpu_instance_create_surface_for_window(int instance, int window);

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window *window) {
    /* Per-window GPU present binding (A4): the window's HOST handle (its
       SDL_WindowID) crosses the import so the host binds THIS surface's
       presents to THIS window's kernel surface, symmetric with the shm
       per-window path. Handle-less wgpuInstanceCreateSurface keeps the
       legacy shared-canvas tail. */
    if (!window) return NULL;
    return (WGPUSurface)__wgpu_instance_create_surface_for_window(
        (int)instance, (int)SDL_GetWindowID(window));
}
  