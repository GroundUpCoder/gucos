
#include <SDL.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Opaque to user code (only the forward declaration is in SDL.h).
   'handle' is a positive integer owned by the selected host backend.
   We reuse it as the SDL window ID (SDL_GetWindowID returns it,
   and event windowID fields carry it). This is fine because we
   control the entire stack — the platform SDL window ID
   never leaks to C code. The struct + registry decl live in
   __SDL_internal.h since docs/archive/0256 (shared with __SDL_popup.c). */
#include <__SDL_internal.h>

/* Window registry: __sdl_push_window_event must find the SDL_Window by
   handle to re-derive its surface on RESIZED (docs/archive/0019). Windows are
   few; a small fixed table with linear scans is fine. A window past the
   cap still works — it just never re-derives on resize. The ARRAY is
   non-static since docs/archive/0256 (declared in __SDL_internal.h): the popup TU
   registers its windows by slotting into it directly, so RESIZED
   re-derivation covers popups too — while the register/unregister/lookup
   helpers stay static (a second reference would defeat their single-use
   inlining and change every SDL binary's bytes; the interlock forbids it). */
SDL_Window *__sdl_window_registry[__SDL_MAX_WINDOWS];

static void __sdl_window_register(SDL_Window *w) {
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++) {
        if (!__sdl_window_registry[i]) { __sdl_window_registry[i] = w; return; }
    }
}

static void __sdl_window_unregister(SDL_Window *w) {
    __sdl_forget_window(w);
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++) {
        if (__sdl_window_registry[i] == w) { __sdl_window_registry[i] = 0; return; }
    }
}

static SDL_Window *__sdl_window_by_handle(int handle) {
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++) {
        SDL_Window *w = __sdl_window_registry[i];
        if (w && w->handle == handle) return w;
    }
    return 0;
}

/* Liveness checks (#497): use of a destroyed window/texture must fail with an
   SDL error, never silently "succeed" against freed memory. A window is live
   iff it is in the registry (SDL_DestroyWindow unregisters before freeing, so
   the check never dereferences the candidate pointer). Textures carry a magic
   tag instead (there is no texture registry) — best-effort by nature, see the
   SDL_Texture struct note. */
static bool __sdl_window_live(SDL_Window *w) {
    if (!w) return 0;
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++)
        if (__sdl_window_registry[i] == w) return 1;
    return 0;
}

#define __SDL_TEX_MAGIC 0x53544558u  /* 'STEX' */
static bool __sdl_texture_live(SDL_Texture *t) {
    return t && t->__magic == __SDL_TEX_MAGIC;
}

/* Low-level host imports — all operate on primitive values only.
   The host (host.js) knows nothing about C struct layouts. */
__import int __sdl_init(int flags);
__import int __sdl_init_subsystem(int flags);
__import void __sdl_quit_subsystem(int flags);
__import void __sdl_quit(void);
__import int __sdl_set_window_visible(int handle, int visible);
__import int __sdl_raise_window(int handle);
__import int __sdl_get_window_state(int handle);
__import int __sdl_set_window_parent(int handle, int parent_handle);
__import int __sdl_get_window_viewable(int handle);
__import int __sdl_create_window(const char *title, int x, int y, int w, int h, int flags);
__import void __sdl_destroy_window(int handle);
__import void __sdl_set_window_title(int handle, const char *title);
__import void __sdl_set_relative_mouse_mode(int handle, int enabled);
/* Per-surface cursor (docs/archive/0105): shape is an SDL_SystemCursor value, or
   -1 to hide (CSS cursor:none). handle names the process's window. */
__import void __sdl_set_cursor(int handle, int shape);
__import int __sdl_set_window_size(int handle, int w, int h);
__import int __sdl_update_window_surface(int handle, const void *pixels, int w, int h, int pitch);
/* A host that can display the window's own BGRX/BGRA surface takes it as is:
   no conversion pass and no second frame buffer. Others keep the RGBA call. */
__import int __sdl_window_surface_bgra(int handle);
__import int __sdl_update_window_surface_bgra(int handle, const void *pixels, int w, int h, int pitch, int opaque);
__import void __sdl_delay(int ms);
/* Blocking input park (docs/archive/0161; host.js pumpWait — the same seam user32's
   GetMessage uses): drain the OS input ring into the wasm event queue and, if
   dry, park on the ring until the kernel's next push or timeoutMs. Returns 1
   if a ring exists (a window was created), 0 otherwise (caller paces itself).
   Wakes can be spurious; callers re-poll their queues. */
__import int __sdl_pump_wait(int timeoutMs);
/* Non-blocking ring pump (ticket #485; host.js drainInput): move any pending
   OS input-ring records into the wasm event queue and return immediately —
   never parks, never touches the frame-idle gate (unlike __sdl_pump_wait(0),
   whose entry releases the compositor pin — a "back to waiting" signal that
   would be wrong from a hot poll loop). SDL_PollEvent's entry pump, so a
   poll-only loop (no SDL_Delay / SDL_WaitEvent anywhere — the most common
   SDL main-loop idiom) still receives input. Returns the record count
   drained (0 when dry or no ring); the dry-ring cost is ~two atomic loads. */
__import int __sdl_pump(void);
/* Gamepad name query (#607): fills buf (NUL-terminated, cap permitting) and
   returns the pad name's full byte length, or -1 for an instance id the
   kernel never allocated. Names outlive disconnect kernel-side, so an open
   handle can still answer SDL_GetGamepadName after REMOVED. Kernel-less
   runtimes (standalone pages, null SDL) return -1. */
__import int __sdl_gamepad_name(int instance_id, char *buf, int cap);
/* libc's sleep import (this unit doesn't include time.h) — the no-ring
   fallback pace in SDL_WaitEventTimeout. */
__import int __nanosleep(long sec, long nsec);
/* ms since SDL_Init as an f64 — FRACTIONAL (performance.now()-backed, #495),
   monotonic. SDL_GetTicks truncates to whole ms (full Uint64, no 32-bit wrap);
   SDL_GetTicksNS / SDL_GetPerformanceCounter and the event timestamps keep the
   sub-ms part. */
__import double __sdl_get_ticks(void);
__import void __sdl_set_animation_frame_func(void (*callback)(void));
/* System clipboard (docs/archive/0090; host.js createClipboard). __clip_has is
   the PEEK: same total/-1 answer as a cap-0 __clip_get but served from the
   cached slot — it never triggers the kernel's deferred host-clipboard
   refresh (the clipboard seam), so availability probes (menu graying)
   can't raise host paste UI. */
__import int __clip_set(int fmt, const void *bytes, int len);
__import int __clip_get(int fmt, void *out, int cap);
__import int __clip_has(int fmt);
/* HTTP transport (docs/archive/0172; fd-shaped since docs/archive/0417 — host.js
   createHttp). The libcurl veneer (0173) sits on these. headers is a
   NUL-terminated blob of "Name: Value" lines joined by newlines (or
   empty). __http_open starts the transfer and returns an ORDINARY fd.
   headers_ms bounds the response headers, idle_ms bounds the gap between
   body bytes (0 = kernel default; idle_ms < 0 disables the idle
   deadline); an expired deadline fails the transfer with ETIMEDOUT.
   Consumer contract (WAIT-first, the os/fswatch.h discipline): park on
   the fd (__wait / select). On a wake, call __http_status once if you
   have not (-1/EAGAIN before the headers arrive; the call consumes the
   status), then read() until EAGAIN, then park again. read() gives
   n > 0 bytes, 0 at clean EOF, -1/errno on failure. close(fd) aborts
   the transfer. A consumer that does not consume its pending status
   spins — that is the caller's bug.
   __http_error (#392) is the error-TEXT peek: the transfer's recorded
   failure text (the transport's own diagnostic — a CORS denial, a dead
   bridge, the fetch cause chain), NUL-terminated into buf; returns the
   total text bytes (0 while healthy), never consumes anything. Read it
   BEFORE close(fd) — the last release frees the transfer, text
   included. -1/ENOSYS on an embedder predating the op: degrade to
   errno-derived text, never crash. */
__import int __http_open(const char *method, const char *url, const char *headers, const void *body, int blen, int headers_ms, int idle_ms);
__import int __http_status(int fd, int *status_out, char *hdr, int hdrcap);
__import int __http_error(int fd, char *buf, int cap);
__import int __sdl_open_audio_device(int freq, int format, int channels);
__import int __sdl_queue_audio(int dev, const void *data, int len);
__import int __sdl_get_queued_audio_size(int dev);
__import void __sdl_clear_queued_audio(int dev);
__import void __sdl_pause_audio_device(int dev, int pause_on);
__import void __sdl_close_audio_device(int dev);

/* SDL_Renderer primitives. Colors are 0..1 floats; the single draw primitive
   takes 4 dst corners (TL,TR,BR,BL in pixels) + a src rect (texture pixels). */
__import int  __sdl_create_renderer(int window, int software);
__import void __sdl_destroy_renderer(int r);
__import int  __sdl_create_texture(int r, int access, int w, int h);
__import void __sdl_destroy_texture(int t);
__import void __sdl_update_texture(int t, const void *pixels, int pitch, int x, int y, int w, int h);
__import void __sdl_set_texture_color_mod(int t, double r, double g, double b);
__import void __sdl_set_texture_alpha_mod(int t, double a);
__import void __sdl_set_texture_blend_mode(int t, int mode);
__import int __sdl_get_texture_blend_mode(int t);
__import void __sdl_set_texture_scale_mode(int t, int mode);
__import int __sdl_get_texture_scale_mode(int t);
__import void __sdl_set_draw_color(int r, double rr, double gg, double bb, double aa);
__import void __sdl_set_draw_blend_mode(int r, int mode);
__import void __sdl_set_render_clip_rect(int r, int enabled, int x, int y, int w, int h);
__import int __sdl_get_render_clip(int r, int field);
__import void __sdl_render_clear(int r);
__import void __sdl_render_quad(int r, int texH, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, double sx, double sy, double sw, double sh);
__import void __sdl_render_geometry(int r, int texH, const float *verts, int vertCount);
__import void __sdl_render_present(int r);
__import void __sdl_set_render_target(int r, int t);  /* t 0 = the window (#496) */
__import int __sdl_set_render_vsync(int r, int vsync);  /* 1 accepted / 0 unsupported (#500) */

/* ---- Error handling ----
   Single-threaded runtime ⇒ one global error buffer. SDL_GetError returns it
   verbatim (empty string, never NULL, when no error is set). SDL_SetError
   formats with vsnprintf and returns false; SDL_ClearError returns true. */
static char __sdl_error[1024];

const char *SDL_GetError(void) {
    return __sdl_error;
}

bool SDL_SetError(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(__sdl_error, sizeof(__sdl_error), fmt, ap);
    va_end(ap);
    return 0;   /* SDL3 convention: SDL_SetError always returns false */
}

bool SDL_ClearError(void) {
    __sdl_error[0] = '\0';
    return 1;   /* SDL3 convention: SDL_ClearError always returns true */
}

/* Subsystems this runtime actually backs: VIDEO (WebGPU canvas), AUDIO (Web
   Audio), EVENTS (the event queue needs no device), and GAMEPAD (#607: the
   OS pad registry over the Web Gamepad API / wmctl injection). Joystick-level
   API/haptic/sensor/camera have no backend, so SDL_Init FAILS LOUD on them
   rather than pretending it initialized (Guiding principle 1: behave like SDL
   or fail loud). NB unlike upstream, SDL_INIT_GAMEPAD does NOT imply
   SDL_INIT_JOYSTICK here — there is no joystick subsystem to imply (this
   runtime has no unmapped-device view; declared divergence, see the header's
   gamepad section). When a subsystem's backend lands, widen this mask. */
#define __SDL_SUPPORTED_SUBSYSTEMS (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)

static void __sdl_gamepad_init_added(void);   /* fwd: ADDED synth at init (#607) */

static SDL_InitFlags __sdl_initted = 0;
/* Whether the host __sdl_init (which baselines the SDL_GetTicks clock) has run
   this init cycle. Tracked separately from the subsystem mask: keying the
   one-time baseline on (__sdl_initted == 0) was wrong because SDL_Init(0) leaves
   the mask 0, so a subsequent SDL_Init would re-call the host and reset ticks. */
static bool __sdl_host_inited = 0;

static bool __sdl_do_init(SDL_InitFlags flags) {
    SDL_InitFlags unsupported = flags & ~(SDL_InitFlags)__SDL_SUPPORTED_SUBSYSTEMS;
    if (unsupported) {
        return SDL_SetError(
            "SDL_Init: requested subsystem(s) 0x%X are not supported by this runtime "
            "(supported: SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD). "
            "Joystick-level API/haptic/sensor/camera have no web backend yet.",
            (unsigned)unsupported);
    }
    /* SDL implicitly brings up EVENTS alongside VIDEO/AUDIO/GAMEPAD. */
    if (flags & (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) flags |= SDL_INIT_EVENTS;
    /* Baseline the tick clock exactly once per init cycle. */
    int status = __sdl_host_inited ? __sdl_init_subsystem((int)flags) : __sdl_init((int)flags);
    if (status < 0) return SDL_SetError("SDL_Init: requested subsystem is unavailable on this host (Node: install/build native SDL support)");
    __sdl_host_inited = 1;
    bool gamepadNew = (flags & SDL_INIT_GAMEPAD) && !(__sdl_initted & SDL_INIT_GAMEPAD);
    __sdl_initted |= flags;
    /* Upstream contract: pads already connected at gamepad-subsystem init
       deliver SDL_EVENT_GAMEPAD_ADDED. The registry below tracks connects
       from ring drains whether or not the subsystem is up (#607). */
    if (gamepadNew) __sdl_gamepad_init_added();
    return 1;
}

bool SDL_Init(SDL_InitFlags flags) {
    return __sdl_do_init(flags);
}

bool SDL_InitSubSystem(SDL_InitFlags flags) {
    return __sdl_do_init(flags);
}

void SDL_QuitSubSystem(SDL_InitFlags flags) {
    __sdl_quit_subsystem((int)flags);
    __sdl_initted &= ~flags;
}

SDL_InitFlags SDL_WasInit(SDL_InitFlags flags) {
    /* SDL3: flags==0 returns the full initialized mask; otherwise the subset. */
    return flags ? (__sdl_initted & flags) : __sdl_initted;
}

SDL_Window *SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags) {
    const SDL_WindowFlags supported=SDL_WINDOW_FULLSCREEN|SDL_WINDOW_HIDDEN|SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE;
    if(flags&~supported){SDL_SetError("Unsupported window flags on the VT platform");return NULL;}
    /* SDL3 dropped the x,y args; the host centers the window, so pass 0,0. */
    int handle = __sdl_create_window(title, 0, 0, w, h, (int)flags);
    if (handle <= 0) { SDL_SetError("SDL_CreateWindow: host failed to create a window"); return NULL; }
    int pitch = w * 4;
    SDL_Window *win = (SDL_Window *)malloc(sizeof(SDL_Window));
    if (!win) { __sdl_destroy_window(handle); SDL_SetError("Out of memory"); return NULL; }
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
    if (!win->surface.pixels) { __sdl_destroy_window(handle); free(win); SDL_SetError("Out of memory"); return NULL; }
    memset(win->surface.pixels, 0, pitch * h);
    win->pixels_cap = pitch * h;
    win->present_pixels = NULL;
    win->present_cap = 0;
    win->present_direct = 0;
    win->relative_mouse = 0;
    win->flags = flags;
    win->renderer = NULL;
    win->parent_handle = 0;
    win->close_reason = 0;
    __sdl_window_register(win);
    SDL_SetStringProperty(SDL_GetWindowProperties(win),"gucos.window.title",title?title:"");
    return win;
}

SDL_WindowID SDL_GetWindowID(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return 0; }
    return (SDL_WindowID)window->handle;
}

SDL_Surface *SDL_GetWindowSurface(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return NULL; }
    return &window->surface;
}

bool SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    /* The surface tracks the window size (resize events re-derive it). */
    if (w) *w = window->surface.w;
    if (h) *h = window->surface.h;
    return 1;
}

/* 1 buffer px = 1 screen px on gucOS (#790; see SDL.h): pixels == size. */
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (w) *w = window->surface.w;
    if (h) *h = window->surface.h;
    return 1;
}

float SDL_GetWindowPixelDensity(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return 0.0f; }
    return 1.0f;
}

float SDL_GetWindowDisplayScale(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return 0.0f; }
    return 1.0f;
}

/* Query authoritative lifecycle state; never infer focus from a request. */
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return 0; }
    int state = __sdl_get_window_state(window->handle);
    if (state < 0) return window->flags;
    return (window->flags & ~(SDL_WINDOW_HIDDEN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MINIMIZED)) | (SDL_WindowFlags)state;
}

bool SDL_ShowWindow(SDL_Window *window) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (__sdl_set_window_visible(window->handle, 1) != 0)
        return SDL_SetError("SDL_ShowWindow: window system refused visibility");
    window->flags &= ~SDL_WINDOW_HIDDEN;
    return 1;
}
bool SDL_HideWindow(SDL_Window *window) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (__sdl_set_window_visible(window->handle, 0) != 0)
        return SDL_SetError("SDL_HideWindow: window system refused visibility");
    window->flags |= SDL_WINDOW_HIDDEN;
    return 1;
}
bool SDL_RaiseWindow(SDL_Window *window) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (__sdl_raise_window(window->handle) != 0)
        return SDL_SetError("SDL_RaiseWindow: window system refused activation");
    return 1;
}

/* Owner relation (#794). The kernel validates ownership (same process, no
   cycles, no popups); the veneer only refuses what it can see locally so the
   error names the real reason. */
bool SDL_SetWindowParent(SDL_Window *window, SDL_Window *parent) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (parent && !__sdl_window_live(parent)) return SDL_InvalidParamError("parent");
    if (window->flags & (SDL_WINDOW_POPUP_MENU | SDL_WINDOW_TOOLTIP))
        return SDL_SetError("SDL_SetWindowParent: a popup window's parent is fixed at creation");
    if (parent && (parent->flags & (SDL_WINDOW_POPUP_MENU | SDL_WINDOW_TOOLTIP)))
        return SDL_SetError("SDL_SetWindowParent: a popup window cannot own windows");
    if (parent == window) return SDL_SetError("SDL_SetWindowParent: a window cannot own itself");
    for (SDL_Window *q = parent; q; q = __sdl_window_by_handle(q->parent_handle))
        if (q == window) return SDL_SetError("SDL_SetWindowParent: ownership cycle");
    if (__sdl_set_window_parent(window->handle, parent ? parent->handle : 0) != 0)
        return SDL_SetError("SDL_SetWindowParent: window system refused the owner link");
    window->parent_handle = parent ? parent->handle : 0;
    return 1;
}
SDL_Window *SDL_GetWindowParent(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return NULL; }
    return window->parent_handle ? __sdl_window_by_handle(window->parent_handle) : NULL;
}
SDL_Window *SDL_GetWindowFromID(SDL_WindowID id) {
    SDL_Window *w = __sdl_window_by_handle((int)id);
    if (!w) SDL_SetError("SDL_GetWindowFromID: no window with id %u", (unsigned)id);
    return w;
}
int guc_window_close_reason(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return 0; }
    return window->close_reason;
}
int guc_window_viewable(SDL_Window *window) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return -1; }
    int v = __sdl_get_window_viewable(window->handle);
    if (v < 0) { SDL_SetError("guc_window_viewable: no window-system lifecycle"); return -1; }
    return v;
}

/* Ask the window system for a new size (docs/archive/0068). ASYNC like upstream
   SDL3 under a real WM: success means the request was accepted; the actual
   size change arrives as SDL_EVENT_WINDOW_RESIZED (which re-derives the
   window surface in place — see __sdl_push_window_event). Only the
   kernel-surface runtime honours it; elsewhere this fails loud. */
bool SDL_SetWindowSize(SDL_Window *window, int w, int h) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if (w < 1 || h < 1)
        return SDL_SetError("SDL_SetWindowSize: invalid size %dx%d", w, h);
    if (__sdl_set_window_size(window->handle, w, h) != 0)
        return SDL_SetError("SDL_SetWindowSize: this runtime cannot resize the window");
    return 1;
}

/* Placement is the WM's; accept and succeed (see SDL.h). Validate the handle
   so a NULL window still reports the SDL error a program might check. */
bool SDL_SetWindowPosition(SDL_Window *window, int x, int y) {
    (void)x; (void)y;
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    return 1;
}

/* No taskbar/title icon pipe yet — succeed so an icon-fatal app runs (see
   SDL.h). The surface stays owned by the caller (the game frees it). */
bool SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    (void)icon;
    return 1;
}

/* Free a surface this runtime malloc'd (IMG_Load sets IMG_SURFACE_OWNED).
   Window surfaces (flags==0, embedded in the SDL_Window record) and NULL are
   left alone — matching SDL3, where SDL_DestroySurface ignores NULL and only
   frees heap surfaces. */
typedef struct __attribute__((packed)) { Uint64 value; } __SDLPresentPair;
bool SDL_UpdateWindowSurface(SDL_Window *window) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    SDL_Surface *s = &window->surface;
    if (!window->present_direct) window->present_direct = __sdl_window_surface_bgra(window->handle) > 0 ? 1 : 2;
    if (window->present_direct == 1) {
        if (__sdl_update_window_surface_bgra(window->handle, s->pixels, s->w, s->h, s->pitch, s->format == SDL_PIXELFORMAT_BGRX32) != 0)
            return SDL_SetError("SDL_UpdateWindowSurface: the window has no surface to present");
        return 1;
    }
    /* Match pygame's upstream alpha surfaces while keeping the host ABI RGBA.
       Swizzle once per frame, not once per effect. Never rewrite the app's
       pixels: callers may retain pointers, and transparent alpha must survive. */
    int need = s->pitch * s->h;
    if (need > window->present_cap) {
        void *p = realloc(window->present_pixels, need);
        if (!p) return SDL_SetError("Out of memory");
        window->present_pixels = p;
        window->present_cap = need;
    }
    Uint32 alpha = s->format == SDL_PIXELFORMAT_BGRX32 ? 0xff000000u : 0;
    Uint64 alpha2 = ((Uint64)alpha << 32) | alpha;
    for (int y = 0; y < s->h; y++) {
        const Uint32 *src = (const Uint32 *)((const Uint8 *)s->pixels + y * s->pitch);
        Uint32 *dst = (Uint32 *)((Uint8 *)window->present_pixels + y * s->pitch);
        int x = 0;
        /* Two pixels per 64-bit word; rows are only 4-byte aligned. */
        for (; x + 2 <= s->w; x += 2) {
            Uint64 p = ((const __SDLPresentPair *)(src + x))->value;
            ((__SDLPresentPair *)(dst + x))->value = (p & 0xff00ff00ff00ff00ull) |
                ((p & 0x000000ff000000ffull) << 16) | ((p >> 16) & 0x000000ff000000ffull) | alpha2;
        }
        for (; x < s->w; x++) {
            Uint32 p = src[x];
            dst[x] = (p & 0xff00ff00u) | ((p & 255u) << 16) | ((p >> 16) & 255u) | alpha;
        }
    }
    if (__sdl_update_window_surface(window->handle, window->present_pixels, s->w, s->h, s->pitch) != 0)
        return SDL_SetError("SDL_UpdateWindowSurface: the window has no surface to present");
    return 1;
}

/* Queue and filter state are shared by timer workers and the main pump. */
static pthread_mutex_t __sdl_event_lock=PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
static SDL_EventFilter __sdl_event_filter;
static void *__sdl_event_filter_userdata;
static Uint8 __sdl_event_disabled[65536];
typedef struct EventWatch {struct EventWatch *next;SDL_EventFilter callback;void *userdata;bool removed;} EventWatch;
static EventWatch *__sdl_event_watches;
static int __sdl_event_dispatching;
static void __sdl_collect_watches(void){if(__sdl_event_dispatching)return;EventWatch **p=&__sdl_event_watches;while(*p){EventWatch *w=*p;if(w->removed){*p=w->next;free(w);}else p=&w->next;}}
bool SDL_EventEnabled(Uint32 type){if(type>65535)return 0;pthread_mutex_lock(&__sdl_event_lock);bool enabled=!__sdl_event_disabled[type];pthread_mutex_unlock(&__sdl_event_lock);return enabled;}
void SDL_SetEventEnabled(Uint32 type,bool enabled){if(type>65535)return;pthread_mutex_lock(&__sdl_event_lock);__sdl_event_disabled[type]=!enabled;if(!enabled)SDL_FlushEvent(type);pthread_mutex_unlock(&__sdl_event_lock);}
bool SDL_AddEventWatch(SDL_EventFilter filter,void *userdata){if(!filter)return SDL_SetError("Invalid event watch");EventWatch *w=calloc(1,sizeof(*w));if(!w)return SDL_SetError("Out of memory");w->callback=filter;w->userdata=userdata;pthread_mutex_lock(&__sdl_event_lock);EventWatch **p=&__sdl_event_watches;while(*p)p=&(*p)->next;*p=w;pthread_mutex_unlock(&__sdl_event_lock);return 1;}
void SDL_RemoveEventWatch(SDL_EventFilter filter,void *userdata){pthread_mutex_lock(&__sdl_event_lock);for(EventWatch *w=__sdl_event_watches;w;w=w->next)if(!w->removed&&w->callback==filter&&w->userdata==userdata){w->removed=1;break;}__sdl_collect_watches();pthread_mutex_unlock(&__sdl_event_lock);}
bool SDL_GetEventFilter(SDL_EventFilter *filter,void **userdata){pthread_mutex_lock(&__sdl_event_lock);if(filter)*filter=__sdl_event_filter;if(userdata)*userdata=__sdl_event_filter_userdata;bool exists=__sdl_event_filter!=NULL;pthread_mutex_unlock(&__sdl_event_lock);return exists;}
void SDL_SetEventFilter(SDL_EventFilter filter,void *userdata){pthread_mutex_lock(&__sdl_event_lock);__sdl_event_filter=filter;__sdl_event_filter_userdata=userdata;SDL_FlushEvents(0,65535);pthread_mutex_unlock(&__sdl_event_lock);}
static bool __sdl_filter_event(SDL_Event *event){if(!SDL_EventEnabled(event->type))return 0;pthread_mutex_lock(&__sdl_event_lock);if(__sdl_event_filter&&!__sdl_event_filter(__sdl_event_filter_userdata,event)){pthread_mutex_unlock(&__sdl_event_lock);return 0;}__sdl_event_dispatching++;EventWatch *last=__sdl_event_watches;while(last&&last->next)last=last->next;for(EventWatch *w=__sdl_event_watches;w;w=w->next){if(!w->removed)w->callback(w->userdata,event);if(w==last)break;}__sdl_event_dispatching--;__sdl_collect_watches();pthread_mutex_unlock(&__sdl_event_lock);return 1;}

/* ---- Event queue (freelist-based linked list) ---- */

/* Returned text remains alive through the next event-pump boundary.
   Recycling from a timer worker retires storage, never frees a reader's text. */
typedef struct __SDL_TextBlob {struct __SDL_TextBlob *next;char bytes[1];} __SDL_TextBlob;
static __SDL_TextBlob *__sdl_retired_text;
static __SDL_TextBlob *__sdl_copy_text(const char *text){size_t n=strlen(text);if(n>(size_t)-1-sizeof(__SDL_TextBlob))return NULL;__SDL_TextBlob *b=malloc(sizeof(*b)+n);if(b){b->next=NULL;memcpy(b->bytes,text,n+1);}return b;}
static void __sdl_cleanup_text(void){pthread_mutex_lock(&__sdl_event_lock);__SDL_TextBlob *b=__sdl_retired_text;__sdl_retired_text=NULL;pthread_mutex_unlock(&__sdl_event_lock);while(b){__SDL_TextBlob *next=b->next;free(b);b=next;}}

typedef struct __SDL_EventEntry {
    SDL_Event event;
    __SDL_TextBlob *text;
    struct __SDL_EventEntry *next;
} __SDL_EventEntry;

static __SDL_EventEntry *__sdl_eq_head;
static __SDL_EventEntry *__sdl_eq_tail;
static __SDL_EventEntry *__sdl_eq_free;

static __SDL_EventEntry *__sdl_eq_alloc(void) {
    pthread_mutex_lock(&__sdl_event_lock);
    __SDL_EventEntry *e = __sdl_eq_free;
    if (e) {
        __sdl_eq_free = e->next;
        if(e->text){e->text->next=__sdl_retired_text;__sdl_retired_text=e->text;e->text=NULL;}
    } else {
        e = (__SDL_EventEntry *)calloc(1,sizeof(__SDL_EventEntry));
    }
    e->next = 0;
    pthread_mutex_unlock(&__sdl_event_lock);
    return e;
}

static void __sdl_eq_push_raw(__SDL_EventEntry *e) {
    pthread_mutex_lock(&__sdl_event_lock);
    if (__sdl_eq_tail) {
        __sdl_eq_tail->next = e;
    } else {
        __sdl_eq_head = e;
    }
    __sdl_eq_tail = e;
    pthread_mutex_unlock(&__sdl_event_lock);
}
static void __sdl_eq_push(__SDL_EventEntry *e){if(__sdl_filter_event(&e->event))__sdl_eq_push_raw(e);else{pthread_mutex_lock(&__sdl_event_lock);e->next=__sdl_eq_free;__sdl_eq_free=e;pthread_mutex_unlock(&__sdl_event_lock);}}

/* Event field population. The host passes DOM-derived primitives (scancode/sym,
   key mod + repeat, button, float coords, motion button-mask state, wheel
   direction); everything else SDL stamps itself — timestamp (SDL_GetTicksNS),
   the device 'which' id, relative motion (xrel/yrel), button click-count, and
   the wheel's mouse position — which we derive here so the SDL_Event matches
   upstream's populated fields, not a memset-zeroed shell. */

/* SDL stamps every event with SDL_GetTicksNS(). We have ms (sub-ms precise via
   performance.now()); ns = ms * 1e6. */
static Uint64 __sdl_now_ns(void) { return (Uint64)(__sdl_get_ticks() * 1000000.0); }

/* Single mouse / keyboard in this runtime; SDL uses a nonzero instance id (0 is
   not a valid SDL device id). */
#define __SDL_MOUSE_ID 1u
#define __SDL_KEYBOARD_ID 1u

/* Tracked to derive xrel/yrel, the wheel's mouse_x/y, and the click count. */
static float __sdl_mx = 0, __sdl_my = 0;
static float __sdl_rel_x=0,__sdl_rel_y=0;
static int __sdl_mouse_focus_handle;
static bool __sdl_have_mpos = 0;
/* ---- Input state snapshots (#493) ----
   Backs SDL_GetKeyboardState / SDL_GetModState / SDL_GetMouseState. Updated
   where the host-pumped events are synthesised (the __sdl_push_* exports) —
   this runtime's equivalent of SDL3's "updated by SDL_PumpEvents". The motion
   push latches the host's full button mask, so a release delivered outside
   our windows self-heals on the next motion event. */
static bool __sdl_key_state[SDL_SCANCODE_COUNT];
static SDL_Keymod __sdl_mod_state = 0;
static SDL_MouseButtonFlags __sdl_mouse_button_state = 0;
static Uint64 __sdl_last_click_ns = 0;
static int __sdl_last_click_btn = 0;
static float __sdl_last_click_x = 0, __sdl_last_click_y = 0;
static Uint8 __sdl_click_count = 0;

void __sdl_push_quit_event(int window_id) {
    /* The kernel's close request names a surface (the QUIT ring record
       carries the sid; the host maps it to our handle). With several
       windows live, deliver a per-window SDL_EVENT_WINDOW_CLOSE_REQUESTED
       so a multi-window app (the user32 veneer) closes just that window
       (docs/archive/0089). The only/last window keeps the historical process-wide
       SDL_EVENT_QUIT. Divergence from upstream SDL3 (which sends
       CLOSE_REQUESTED and then QUIT for the last window) is deliberate:
       one event per request, so a queued pair can't double-close. */
    int live = 0;
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++)
        if (__sdl_window_registry[i]) live++;
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    SDL_Window *target = __sdl_window_by_handle(window_id);
    if (target) target->close_reason = GUC_CLOSE_REASON_REQUEST;
    if (live > 1 && target) {
        e->event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        e->event.window.timestamp = __sdl_now_ns();
        e->event.window.windowID = (SDL_WindowID)window_id;
    } else {
        e->event.type = SDL_EVENT_QUIT;
        e->event.common.timestamp = __sdl_now_ns();
    }
    __sdl_eq_push(e);
}
__export __sdl_push_quit_event = __sdl_push_quit_event;

/* A popup grab dismissal (#794, kernel POPUP_DISMISSED): the legacy mapping
   is kept — the popup's window gets SDL_EVENT_WINDOW_CLOSE_REQUESTED exactly
   as before — but the reason is recorded first, so guc_window_close_reason
   can tell this housekeeping event from a user close request. Never a
   process-wide SDL_EVENT_QUIT: a dismissed popup is not the last window. */
void __sdl_push_popup_dismissed(int window_id, int reason) {
    SDL_Window *target = __sdl_window_by_handle(window_id);
    if (!target) return;
    target->close_reason = reason ? reason : GUC_CLOSE_REASON_POPUP_DISMISS;
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    e->event.window.timestamp = __sdl_now_ns();
    e->event.window.windowID = (SDL_WindowID)window_id;
    __sdl_eq_push(e);
}
__export __sdl_push_popup_dismissed = __sdl_push_popup_dismissed;

/* Kernel-WM window events (docs/archive/0019). RESIZED re-derives the window
   surface IN PLACE before the event is queued: w/h/pitch update, but the
   pixel allocation only ever GROWS (high-water) — a program that keeps
   drawing with stale dimensions writes inside the allocation instead of
   corrupting the heap. Per the SDL3 contract, programs should re-fetch
   SDL_GetWindowSurface after a resize event (same pointer here; fields
   are current either way). */
void __sdl_push_window_event(int window_id, int type, int data1, int data2) {
    if(type==SDL_EVENT_WINDOW_MOUSE_ENTER)__sdl_mouse_focus_handle=window_id;
    if(type==SDL_EVENT_WINDOW_MOUSE_LEAVE&&__sdl_mouse_focus_handle==window_id)__sdl_mouse_focus_handle=0;
    if (type == SDL_EVENT_WINDOW_RESIZED && data1 > 0 && data2 > 0) {
        SDL_Window *w = __sdl_window_by_handle(window_id);
        if (w) {
            int pitch = data1 * 4;
            int need = pitch * data2;
            if (need > w->pixels_cap) {
                void *np = realloc(w->surface.pixels, (size_t)need);
                if (!np) return;           /* can't grow: drop the event */
                w->surface.pixels = np;
                w->pixels_cap = need;
            }
            w->surface.w = data1;
            w->surface.h = data2;
            w->surface.pitch = pitch;
            __sdl_surface_resized(&w->surface);
            memset(w->surface.pixels, 0, (size_t)need);
        }
    }
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = (Uint32)type;
    e->event.window.timestamp = __sdl_now_ns();
    e->event.window.windowID = (SDL_WindowID)window_id;
    e->event.window.data1 = data1;
    e->event.window.data2 = data2;
    __sdl_eq_push(e);
}
__export __sdl_push_window_event = __sdl_push_window_event;

void __sdl_push_key_event(int window_id, int type, int scancode, int sym, int mod, int repeat) {
    if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT)
        __sdl_key_state[scancode] = (type == SDL_EVENT_KEY_DOWN);
    __sdl_mod_state = (SDL_Keymod)mod;
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = (Uint32)type;
    e->event.key.timestamp = __sdl_now_ns();
    e->event.key.windowID = (SDL_WindowID)window_id;
    e->event.key.which = __SDL_KEYBOARD_ID;
    e->event.key.down = (type == SDL_EVENT_KEY_DOWN);
    e->event.key.repeat = repeat ? 1 : 0;
    e->event.key.scancode = (SDL_Scancode)scancode;
    e->event.key.key = (SDL_Keycode)sym;
    e->event.key.mod = (SDL_Keymod)mod;
    __sdl_eq_push(e);
    /* DOM keycodes already carry modifier-applied Unicode. Synthesize plain
       text only here, at the explicit safe pump boundary. IME composition is delivered separately by the browser event pump;
       candidate rectangle positioning remains unsupported. */
    SDL_Window *window=__sdl_window_by_handle(window_id);
    Uint32 cp=(Uint32)sym;
    if((cp&0x40000000u)&&(mod&SDL_KMOD_NUM)&&scancode>=89&&scancode<=98)cp=scancode==98?'0':'1'+scancode-89;
    bool shortcut=(mod&SDL_KMOD_GUI)||((mod&SDL_KMOD_CTRL)&&!(mod&(SDL_KMOD_RALT|SDL_KMOD_MODE)));
    if(type==SDL_EVENT_KEY_DOWN&&!shortcut&&cp>=32&&cp!=127&&cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff)&&window&&SDL_TextInputActive(window)){
        char text[8];*SDL_UCS4ToUTF8(cp,text)=0;
        __SDL_EventEntry *t=__sdl_eq_alloc();t->text=__sdl_copy_text(text);
        if(t->text){memset(&t->event,0,sizeof(t->event));t->event.type=SDL_EVENT_TEXT_INPUT;t->event.text.timestamp=__sdl_now_ns();t->event.text.windowID=window_id;t->event.text.text=t->text->bytes;__sdl_eq_push(t);}
        else {pthread_mutex_lock(&__sdl_event_lock);t->next=__sdl_eq_free;__sdl_eq_free=t;pthread_mutex_unlock(&__sdl_event_lock);}
    }
}
__export __sdl_push_key_event = __sdl_push_key_event;

/* Called only at the host's allocator-safe event pump boundary. */
void *__sdl_text_alloc(int size){return size>0&&size<=65536?malloc(size):NULL;}
__export __sdl_text_alloc = __sdl_text_alloc;
void __sdl_push_text_event(int handle,int type,char *text,int start,int length){
    SDL_Window *w=__sdl_window_by_handle(handle);
    if(!text)return;
    if(!w||!SDL_TextInputActive(w)){free(text);return;}
    __SDL_EventEntry *e=__sdl_eq_alloc();
    e->text=__sdl_copy_text(text);free(text);
    if(!e->text){pthread_mutex_lock(&__sdl_event_lock);e->next=__sdl_eq_free;__sdl_eq_free=e;pthread_mutex_unlock(&__sdl_event_lock);return;}
    memset(&e->event,0,sizeof(SDL_Event));
    e->event.type=type;e->event.text.timestamp=__sdl_now_ns();e->event.text.windowID=handle;e->event.text.text=e->text->bytes;
    if(type==SDL_EVENT_TEXT_EDITING){e->event.edit.start=start;e->event.edit.length=length;}
    __sdl_eq_push(e);
}
__export __sdl_push_text_event = __sdl_push_text_event;

void __sdl_push_mouse_button_event(int window_id, int type, int button, double x, double y) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    bool down = (type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    Uint64 now = __sdl_now_ns();
    if (down) {
        /* Click-count: consecutive presses of the same button within 500 ms and a
           small radius accumulate (double/triple click), else reset to 1. SDL
           never reports clicks == 0. */
        if (button == __sdl_last_click_btn &&
            now - __sdl_last_click_ns <= 500000000ULL &&
            fabs(x - __sdl_last_click_x) <= 32.0 &&
            fabs(y - __sdl_last_click_y) <= 32.0) {
            __sdl_click_count++;
        } else {
            __sdl_click_count = 1;
        }
        __sdl_last_click_ns = now;
        __sdl_last_click_btn = button;
        __sdl_last_click_x = (float)x;
        __sdl_last_click_y = (float)y;
    }
    e->event.type = (Uint32)type;
    e->event.button.timestamp = now;
    e->event.button.windowID = (SDL_WindowID)window_id;
    e->event.button.which = __SDL_MOUSE_ID;
    e->event.button.button = (Uint8)button;
    e->event.button.down = down;
    e->event.button.clicks = __sdl_click_count ? __sdl_click_count : 1;
    e->event.button.x = (float)x;
    e->event.button.y = (float)y;
    __sdl_mx = (float)x; __sdl_my = (float)y; __sdl_have_mpos = 1;
    if (button >= 1 && button <= 32) {
        if (down) __sdl_mouse_button_state |= SDL_BUTTON_MASK(button);
        else      __sdl_mouse_button_state &= ~SDL_BUTTON_MASK(button);
    }
    __sdl_eq_push(e);
}
__export __sdl_push_mouse_button_event = __sdl_push_mouse_button_event;

void __sdl_push_mouse_motion_event(int window_id, double x, double y, int state) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    float fx = (float)x, fy = (float)y;
    float xrel = 0, yrel = 0;
    if (__sdl_have_mpos) { xrel = fx - __sdl_mx; yrel = fy - __sdl_my; }
    __sdl_mx = fx; __sdl_my = fy; __sdl_have_mpos = 1;
    __sdl_mouse_button_state = (SDL_MouseButtonFlags)state;
    e->event.type = SDL_EVENT_MOUSE_MOTION;
    e->event.motion.timestamp = __sdl_now_ns();
    e->event.motion.windowID = (SDL_WindowID)window_id;
    e->event.motion.which = __SDL_MOUSE_ID;
    e->event.motion.state = (Uint32)state;
    e->event.motion.x = fx;
    e->event.motion.y = fy;
    __sdl_mouse_focus_handle=window_id;__sdl_rel_x+=xrel;__sdl_rel_y+=yrel;
    e->event.motion.xrel = xrel;
    e->event.motion.yrel = yrel;
    __sdl_eq_push(e);
}
__export __sdl_push_mouse_motion_event = __sdl_push_mouse_motion_event;

/* Relative-mode motion (docs/archive/0018): the host passes TRUE deltas (pointer-lock
   movementX/Y or an injected rel record) — x/y stay at the last tracked
   position (SDL3 semantics: the position freezes while relative mode is on)
   and the tracked position is NOT advanced by deltas. */
void __sdl_push_mouse_motion_rel_event(int window_id, double dx, double dy, int state) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = SDL_EVENT_MOUSE_MOTION;
    e->event.motion.timestamp = __sdl_now_ns();
    e->event.motion.windowID = (SDL_WindowID)window_id;
    e->event.motion.which = __SDL_MOUSE_ID;
    e->event.motion.state = (Uint32)state;
    e->event.motion.x = __sdl_mx;
    e->event.motion.y = __sdl_my;
    __sdl_mouse_focus_handle=window_id;__sdl_rel_x+=(float)dx;__sdl_rel_y+=(float)dy;
    e->event.motion.xrel = (float)dx;
    e->event.motion.yrel = (float)dy;
    __sdl_eq_push(e);
}
__export __sdl_push_mouse_motion_rel_event = __sdl_push_mouse_motion_rel_event;

void __sdl_push_mouse_wheel_event(int window_id, double x, double y, int direction) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = SDL_EVENT_MOUSE_WHEEL;
    e->event.wheel.timestamp = __sdl_now_ns();
    e->event.wheel.windowID = (SDL_WindowID)window_id;
    e->event.wheel.which = __SDL_MOUSE_ID;
    e->event.wheel.x = (float)x;
    e->event.wheel.y = (float)y;
    e->event.wheel.direction = (Uint32)direction;
    /* SDL fills the wheel event with the current mouse position. */
    e->event.wheel.mouse_x = __sdl_mx;
    e->event.wheel.mouse_y = __sdl_my;
    __sdl_eq_push(e);
}
__export __sdl_push_mouse_wheel_event = __sdl_push_mouse_wheel_event;

/* ---- Gamepads (#607) ----
   The registry mirrors what the kernel routed to this process: the pushes
   below (called by host.js drainInput on GAMEPAD ring records) keep it
   current whether or not the subsystem is up, and event queueing follows
   the upstream gates — ADDED/REMOVED once SDL_INIT_GAMEPAD is up (with
   events enabled), BUTTON/AXIS additionally only for pads this app OPENED.
   SDL_Gamepad (opaque in the header) is a pointer to a registry slot;
   instance ids are kernel-allocated, monotonic, never reused, so a slot is
   reusable the moment it is disconnected AND closed. */

#define __SDL_MAX_GAMEPADS 8

struct SDL_Gamepad {
    Uint32 id;                          /* 0 = free slot */
    bool connected;
    int opened;                         /* SDL_OpenGamepad refcount */
    Uint32 buttons;                     /* bitmask by SDL_GamepadButton */
    Sint16 axes[SDL_GAMEPAD_AXIS_COUNT];
    bool name_fetched;
    char name[128];
};

static struct SDL_Gamepad __sdl_gamepads[__SDL_MAX_GAMEPADS];
static bool __sdl_gamepad_events_on = 1;

static struct SDL_Gamepad *__sdl_pad_by_id(Uint32 id) {
    if (!id) return NULL;
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (__sdl_gamepads[i].id == id) return &__sdl_gamepads[i];
    return NULL;
}

/* Validate an app-supplied handle: must point at a live slot. */
static struct SDL_Gamepad *__sdl_pad_valid(SDL_Gamepad *g) {
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (g == &__sdl_gamepads[i] && __sdl_gamepads[i].id) return &__sdl_gamepads[i];
    SDL_SetError("Invalid gamepad");
    return NULL;
}

static void __sdl_pad_free_if_done(struct SDL_Gamepad *p) {
    if (!p->connected && p->opened <= 0) memset(p, 0, sizeof(*p));
}

static void __sdl_gq_push_device(Uint32 type, Uint32 id) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = type;
    e->event.gdevice.timestamp = __sdl_now_ns();
    e->event.gdevice.which = (SDL_JoystickID)id;
    __sdl_eq_push(e);
}

static bool __sdl_gamepad_events_live(void) {
    return (__sdl_initted & SDL_INIT_GAMEPAD) && __sdl_gamepad_events_on;
}

/* ADDED synth at SDL_Init(SDL_INIT_GAMEPAD) — the upstream already-connected
   contract; the forward declaration sits next to __SDL_SUPPORTED_SUBSYSTEMS. */
static void __sdl_gamepad_init_added(void) {
    if (!__sdl_gamepad_events_on) return;
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (__sdl_gamepads[i].id && __sdl_gamepads[i].connected)
            __sdl_gq_push_device(SDL_EVENT_GAMEPAD_ADDED, __sdl_gamepads[i].id);
}

void __sdl_push_gamepad_added(int id) {
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)id);
    if (p) { p->connected = 1; return; }        /* duplicate ADDED: idempotent */
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++) {
        if (__sdl_gamepads[i].id) continue;
        p = &__sdl_gamepads[i];
        memset(p, 0, sizeof(*p));
        p->id = (Uint32)id;
        p->connected = 1;
        if (__sdl_gamepad_events_live())
            __sdl_gq_push_device(SDL_EVENT_GAMEPAD_ADDED, p->id);
        return;
    }
    /* More than __SDL_MAX_GAMEPADS live pads: drop (documented cap). */
}
__export __sdl_push_gamepad_added = __sdl_push_gamepad_added;

void __sdl_push_gamepad_removed(int id) {
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)id);
    if (!p || !p->connected) return;
    p->connected = 0;
    p->buttons = 0;                              /* a gone pad reads all-zero */
    memset(p->axes, 0, sizeof(p->axes));
    if (__sdl_gamepad_events_live())
        __sdl_gq_push_device(SDL_EVENT_GAMEPAD_REMOVED, p->id);
    __sdl_pad_free_if_done(p);                   /* open handles keep the slot */
}
__export __sdl_push_gamepad_removed = __sdl_push_gamepad_removed;

void __sdl_push_gamepad_button(int id, int button, int down) {
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)id);
    if (!p || !p->connected || button < 0 || button >= 32) return;
    Uint32 bit = 1u << button;
    if (down) p->buttons |= bit; else p->buttons &= ~bit;
    if (!__sdl_gamepad_events_live() || p->opened <= 0) return;
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    e->event.gbutton.timestamp = __sdl_now_ns();
    e->event.gbutton.which = (SDL_JoystickID)p->id;
    e->event.gbutton.button = (Uint8)button;
    e->event.gbutton.down = down ? 1 : 0;
    __sdl_eq_push(e);
}
__export __sdl_push_gamepad_button = __sdl_push_gamepad_button;

void __sdl_push_gamepad_axis(int id, int axis, int value) {
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)id);
    if (!p || !p->connected || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return;
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    p->axes[axis] = (Sint16)value;
    if (!__sdl_gamepad_events_live() || p->opened <= 0) return;
    __SDL_EventEntry *e = __sdl_eq_alloc();
    memset(&e->event, 0, sizeof(SDL_Event));
    e->event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e->event.gaxis.timestamp = __sdl_now_ns();
    e->event.gaxis.which = (SDL_JoystickID)p->id;
    e->event.gaxis.axis = (Uint8)axis;
    e->event.gaxis.value = (Sint16)value;
    __sdl_eq_push(e);
}
__export __sdl_push_gamepad_axis = __sdl_push_gamepad_axis;

/* The public gamepad API. Everything below requires the subsystem; the
   init check fails loud instead of reading a registry the app never asked
   to exist (upstream's SDL_JoystickList behaves the same way). */

static bool __sdl_gamepad_initted(void) {
    if (__sdl_initted & SDL_INIT_GAMEPAD) return 1;
    SDL_SetError("Gamepad subsystem is not initialized (SDL_Init(SDL_INIT_GAMEPAD))");
    return 0;
}

SDL_JoystickID *SDL_GetGamepads(int *count) {
    if (count) *count = 0;
    if (!__sdl_gamepad_initted()) return NULL;
    __sdl_pump();                    /* fresh hotplug view, the PollEvent rule */
    int n = 0;
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (__sdl_gamepads[i].id && __sdl_gamepads[i].connected) n++;
    SDL_JoystickID *ids = (SDL_JoystickID *)SDL_malloc((n + 1) * sizeof(SDL_JoystickID));
    if (!ids) { SDL_SetError("Out of memory"); return NULL; }
    int k = 0;
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (__sdl_gamepads[i].id && __sdl_gamepads[i].connected)
            ids[k++] = (SDL_JoystickID)__sdl_gamepads[i].id;
    ids[k] = 0;
    if (count) *count = n;
    return ids;
}

bool SDL_HasGamepad(void) {
    if (!(__sdl_initted & SDL_INIT_GAMEPAD)) return 0;
    __sdl_pump();
    for (int i = 0; i < __SDL_MAX_GAMEPADS; i++)
        if (__sdl_gamepads[i].id && __sdl_gamepads[i].connected) return 1;
    return 0;
}

bool SDL_IsGamepad(SDL_JoystickID instance_id) {
    /* Every device here IS a gamepad (the browser already mapped it). */
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)instance_id);
    return p && p->connected;
}

SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID instance_id) {
    if (!__sdl_gamepad_initted()) return NULL;
    __sdl_pump();
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)instance_id);
    if (!p || !p->connected) {
        SDL_SetError("SDL_OpenGamepad: invalid gamepad instance id %u",
                     (unsigned)instance_id);
        return NULL;
    }
    p->opened++;
    return p;
}

void SDL_CloseGamepad(SDL_Gamepad *gamepad) {
    struct SDL_Gamepad *p = gamepad ? __sdl_pad_valid(gamepad) : NULL;
    if (!p || p->opened <= 0) return;
    p->opened--;
    __sdl_pad_free_if_done(p);
}

SDL_Gamepad *SDL_GetGamepadFromID(SDL_JoystickID instance_id) {
    struct SDL_Gamepad *p = __sdl_pad_by_id((Uint32)instance_id);
    if (p && p->opened > 0) return p;
    SDL_SetError("SDL_GetGamepadFromID: gamepad %u has not been opened",
                 (unsigned)instance_id);
    return NULL;
}

SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad *gamepad) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    return p ? (SDL_JoystickID)p->id : 0;
}

const char *SDL_GetGamepadName(SDL_Gamepad *gamepad) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    if (!p) return NULL;
    if (!p->name_fetched) {
        if (__sdl_gamepad_name((int)p->id, p->name, (int)sizeof(p->name)) < 0)
            p->name[0] = '\0';
        p->name_fetched = 1;
    }
    return p->name;
}

const char *SDL_GetGamepadNameForID(SDL_JoystickID instance_id) {
    /* Static-buffer return, valid until the next call — works for pads this
       app never opened (the kernel's name cache outlives disconnect). */
    static char nameBuf[128];
    if (__sdl_gamepad_name((int)instance_id, nameBuf, (int)sizeof(nameBuf)) < 0) {
        SDL_SetError("SDL_GetGamepadNameForID: unknown gamepad instance id %u",
                     (unsigned)instance_id);
        return NULL;
    }
    return nameBuf;
}

bool SDL_GamepadConnected(SDL_Gamepad *gamepad) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    return p && p->connected;
}

SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *gamepad) {
    return __sdl_pad_valid(gamepad) ? SDL_GAMEPAD_TYPE_STANDARD
                                    : SDL_GAMEPAD_TYPE_UNKNOWN;
}

bool SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    if (!p || !p->connected || button < 0 || (int)button >= 32) return 0;
    return (p->buttons >> (int)button) & 1u;
}

Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    if (!p || !p->connected || axis < 0 || (int)axis >= SDL_GAMEPAD_AXIS_COUNT) return 0;
    return p->axes[(int)axis];
}

/* The web standard mapping carries SDL buttons 0..14 (SOUTH..DPAD_RIGHT —
   the W3C triggers surface as the trigger AXES) and all six axes. */
bool SDL_GamepadHasButton(SDL_Gamepad *gamepad, SDL_GamepadButton button) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    return p && button >= SDL_GAMEPAD_BUTTON_SOUTH && button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
}

bool SDL_GamepadHasAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis) {
    struct SDL_Gamepad *p = __sdl_pad_valid(gamepad);
    return p && axis > SDL_GAMEPAD_AXIS_INVALID && axis < SDL_GAMEPAD_AXIS_COUNT;
}

void SDL_UpdateGamepads(void) {
    __sdl_pump();
}

/* Button/axis name tables — upstream SDL3 strings verbatim (SDL_gamepad.c
   map_StringForGamepadButton: the mapping-DB vocabulary, so SOUTH is "a"),
   index == enum value. */
static const char *const __sdl_gamepad_button_names[] = {
    "a", "b", "x", "y", "back", "guide", "start",
    "leftstick", "rightstick", "leftshoulder", "rightshoulder",
    "dpup", "dpdown", "dpleft", "dpright", "misc1",
    "paddle1", "paddle2", "paddle3", "paddle4", "touchpad",
    "misc2", "misc3", "misc4", "misc5", "misc6",
};
static const char *const __sdl_gamepad_axis_names[] = {
    "leftx", "lefty", "rightx", "righty", "lefttrigger", "righttrigger",
};

static bool __sdl_strieq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

const char *SDL_GetGamepadStringForButton(SDL_GamepadButton button) {
    if (button <= SDL_GAMEPAD_BUTTON_INVALID || button >= SDL_GAMEPAD_BUTTON_COUNT)
        return NULL;
    return __sdl_gamepad_button_names[(int)button];
}

SDL_GamepadButton SDL_GetGamepadButtonFromString(const char *str) {
    if (str) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
            if (__sdl_strieq(str, __sdl_gamepad_button_names[i]))
                return (SDL_GamepadButton)i;
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

const char *SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis) {
    if (axis <= SDL_GAMEPAD_AXIS_INVALID || axis >= SDL_GAMEPAD_AXIS_COUNT)
        return NULL;
    return __sdl_gamepad_axis_names[(int)axis];
}

SDL_GamepadAxis SDL_GetGamepadAxisFromString(const char *str) {
    if (str) {
        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
            if (__sdl_strieq(str, __sdl_gamepad_axis_names[i]))
                return (SDL_GamepadAxis)i;
    }
    return SDL_GAMEPAD_AXIS_INVALID;
}

void SDL_SetGamepadEventsEnabled(bool enabled) {
    __sdl_gamepad_events_on = enabled;
}

bool SDL_GamepadEventsEnabled(void) {
    return __sdl_gamepad_events_on;
}

static bool __sdl_poll_queued(SDL_Event *event) {
    /* Upstream SDL3 pumps inside SDL_PollEvent; without this a poll-only
       loop never sees ring input at all — the window is unclosable and the
       app unquittable (ticket #485). */
    __SDL_EventEntry *e = __sdl_eq_head;
    if (!e) return 0;
    /* SDL3: a NULL event peeks — return true if one is queued, but do NOT remove
       it from the queue (and never dereference the NULL pointer). */
    if (!event) return 1;
    __sdl_eq_head = e->next;
    if (!__sdl_eq_head) __sdl_eq_tail = 0;
    *event = e->event;
    e->next = __sdl_eq_free;
    __sdl_eq_free = e;
    return 1;
}

bool SDL_PollEvent(SDL_Event *event){__sdl_cleanup_text();__sdl_pump();pthread_mutex_lock(&__sdl_event_lock);bool result=__sdl_poll_queued(event);pthread_mutex_unlock(&__sdl_event_lock);return result;}

/* SDL_WaitEvent / SDL_WaitEventTimeout (docs/archive/0161, IDLE-POWER Stage 2 —
   the real SDL idiom for event-driven apps: block, don't poll).
   Parks on the OS input ring via __sdl_pump_wait (host.js pumpWait, the seam
   user32's blocking GetMessage has used since 0058), so a waiting app is off
   the vsync heartbeat entirely: the kernel wakes it on routed input, resize,
   quit — never 60x/s for nothing. The park is CHUNKED at 1s per import call
   because an env-import RETURN is the cooperative-signal safe point; one
   unbounded park would defer SIGTERM and friends forever. Each chunk wake
   re-polls and re-parks, so an idle waiter costs ~1 wakeup/s.
   SDL3 semantics kept: a NULL event peeks (the event stays queued for the
   next SDL_PollEvent); timeoutMS < 0 waits forever; returns false on timeout.
   Without an input ring (no window yet, or a flavor with no kernel behind
   it) there is nothing to park on — nanosleep the chunk so the timeout
   semantics hold instead of a hot spin. */
bool SDL_WaitEventTimeout(SDL_Event *event, Sint32 timeoutMS) {
    Uint64 deadline = 0;
    if (timeoutMS > 0) deadline = SDL_GetTicks() + (Uint64)timeoutMS;
    for (;;) {
        if (SDL_PollEvent(event)) return 1;
        if (timeoutMS == 0) return 0;
        int chunk = 1000;
        if (timeoutMS > 0) {
            Uint64 now = SDL_GetTicks();
            if (now >= deadline) return 0;
            Uint64 left = deadline - now;
            if (left < (Uint64)chunk) chunk = (int)left;
        }
        /* Worker timers enqueue in shared memory, outside the OS input ring.
           Bound that ring wait while timers are live so their events wake a
           waiting consumer without an asynchronous host reentry. */
        chunk=__sdl_timer_wait_bound(chunk);
        if (!__sdl_pump_wait(chunk))
            __nanosleep(chunk / 1000, (long)(chunk % 1000) * 1000000L);
    }
}

bool SDL_WaitEvent(SDL_Event *event) {
    return SDL_WaitEventTimeout(event, -1);
}

/* ---- Event-queue manipulation (#604) ----
   All over the freelist queue above. SDL_PumpEvents is the explicit
   non-blocking ring drain (the #485 primitive SDL_PollEvent pumps through),
   which also advances the #493 input-state snapshots — the push exports ARE
   this runtime's "updated by SDL_PumpEvents" point. The scan/flush entries
   operate on the queue as-is (upstream semantics: they do not pump).
   Single-threaded runtime: SDL_PushEvent can never race a parked
   SDL_WaitEvent — the wait loop re-polls before every park, so a pushed
   event completes the next wait without parking. */

void SDL_PumpEvents(void) {
    __sdl_cleanup_text();
    __sdl_pump();
}

static bool __sdl_eq_type_in(Uint32 type, Uint32 minType, Uint32 maxType) {
    return type >= minType && type <= maxType;
}

/* Append a copy of *event, stamping a zero timestamp (upstream SDL_AddEvent). */
static void __sdl_eq_add(const SDL_Event *event) {
    __SDL_EventEntry *e = __sdl_eq_alloc();
    e->event = *event;
    if((event->type==SDL_EVENT_TEXT_INPUT||event->type==SDL_EVENT_TEXT_EDITING)&&event->text.text){e->text=__sdl_copy_text(event->text.text);if(e->text)e->event.text.text=e->text->bytes;else e->event.text.text="";}
    if (!e->event.common.timestamp) e->event.common.timestamp = __sdl_now_ns();
    __sdl_eq_push_raw(e);
}

bool SDL_PushEvent(SDL_Event *event) {
    if (!event) return SDL_InvalidParamError("event");
    if(!event->common.timestamp)event->common.timestamp=__sdl_now_ns();
    if(!__sdl_filter_event(event))return 0;
    __sdl_eq_add(event);
    return 1;
}

/* Hand out types from the [SDL_EVENT_USER, SDL_EVENT_LAST] range; 0 on a
   nonpositive or unsatisfiable request (upstream's exhaustion contract). */
static Uint32 __sdl_next_user_event = SDL_EVENT_USER;

static Uint32 __locked_SDL_RegisterEvents(int numevents) {
    if (numevents <= 0) return 0;
    if ((Uint32)numevents > (Uint32)(SDL_EVENT_LAST - __sdl_next_user_event) + 1) return 0;
    Uint32 first = __sdl_next_user_event;
    __sdl_next_user_event += (Uint32)numevents;
    return first;
}
Uint32 SDL_RegisterEvents(int numevents){pthread_mutex_lock(&__sdl_event_lock);Uint32 result=__locked_SDL_RegisterEvents(numevents);pthread_mutex_unlock(&__sdl_event_lock);return result;}

static int __locked_SDL_PeepEvents(SDL_Event *events, int numevents, SDL_EventAction action,
                   Uint32 minType, Uint32 maxType) {
    if (action == SDL_ADDEVENT) {
        if (!events) { SDL_InvalidParamError("events"); return -1; }
        for (int i = 0; i < numevents; i++)
            __sdl_eq_add(&events[i]);
        return numevents > 0 ? numevents : 0;
    }
    if (action != SDL_PEEKEVENT && action != SDL_GETEVENT) {
        SDL_InvalidParamError("action");
        return -1;
    }
    /* NULL events = a counting peek capped at one — upstream's exact rule
       (SDL_HasEvents only needs "is there at least one"). */
    SDL_Event tmp;
    if (!events) { events = &tmp; numevents = 1; action = SDL_PEEKEVENT; }
    int used = 0;
    __SDL_EventEntry *e = __sdl_eq_head, *prev = 0;
    while (e && used < numevents) {
        __SDL_EventEntry *next = e->next;
        if (__sdl_eq_type_in(e->event.type, minType, maxType)) {
            events[used++] = e->event;
            if (action == SDL_GETEVENT) {
                if (prev) prev->next = next; else __sdl_eq_head = next;
                if (__sdl_eq_tail == e) __sdl_eq_tail = prev;
                e->next = __sdl_eq_free;
                __sdl_eq_free = e;
                e = next;
                continue;
            }
        }
        prev = e;
        e = next;
    }
    return used;
}
int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_EventAction action,
                   Uint32 minType, Uint32 maxType){pthread_mutex_lock(&__sdl_event_lock);int result=__locked_SDL_PeepEvents(events,numevents,action,minType,maxType);pthread_mutex_unlock(&__sdl_event_lock);return result;}

static bool __locked_SDL_HasEvents(Uint32 minType, Uint32 maxType) {
    for (__SDL_EventEntry *e = __sdl_eq_head; e; e = e->next)
        if (__sdl_eq_type_in(e->event.type, minType, maxType)) return 1;
    return 0;
}
bool SDL_HasEvents(Uint32 minType, Uint32 maxType){pthread_mutex_lock(&__sdl_event_lock);bool result=__locked_SDL_HasEvents(minType,maxType);pthread_mutex_unlock(&__sdl_event_lock);return result;}

bool SDL_HasEvent(Uint32 type) {
    return SDL_HasEvents(type, type);
}

static void __locked_SDL_FlushEvents(Uint32 minType, Uint32 maxType) {
    __SDL_EventEntry *e = __sdl_eq_head, *prev = 0;
    while (e) {
        __SDL_EventEntry *next = e->next;
        if (__sdl_eq_type_in(e->event.type, minType, maxType)) {
            if (prev) prev->next = next; else __sdl_eq_head = next;
            if (__sdl_eq_tail == e) __sdl_eq_tail = prev;
            e->next = __sdl_eq_free;
            __sdl_eq_free = e;
        } else {
            prev = e;
        }
        e = next;
    }
}
void SDL_FlushEvents(Uint32 minType, Uint32 maxType){pthread_mutex_lock(&__sdl_event_lock);__locked_SDL_FlushEvents(minType,maxType);pthread_mutex_unlock(&__sdl_event_lock);}

void SDL_FilterEvents(SDL_EventFilter filter,void *userdata){if(!filter)return;pthread_mutex_lock(&__sdl_event_lock);__SDL_EventEntry *e=__sdl_eq_head,*prev=NULL;while(e){__SDL_EventEntry *next=e->next;if(!filter(userdata,&e->event)){if(prev)prev->next=next;else __sdl_eq_head=next;if(__sdl_eq_tail==e)__sdl_eq_tail=prev;e->next=__sdl_eq_free;__sdl_eq_free=e;}else prev=e;e=next;}pthread_mutex_unlock(&__sdl_event_lock);}

void SDL_FlushEvent(Uint32 type) {
    SDL_FlushEvents(type, type);
}

/* ---- Audio ----
   SDL3 replaced the SDL2 device + queue API with SDL_AudioStream. Two stream
   kinds share one public type (#529-A, ticket #722):

   DEVICE streams preserve the original push transport until a get callback
   is installed on an empty stream. Pull mode upgrades to a ring in shared
   Wasm memory and a dedicated pthread that requests input under the stream
   lock. SDL3 streams start paused, so callers must
   SDL_ResumeAudioStreamDevice to hear output — same as the old
   SDL_PauseAudioDevice(dev, 0). What doesn't fit the fixed SAB ring is held
   in a C-side backlog and re-pumped (SDL_AudioStream is an UNBOUNDED queue —
   src/audio/SDL_audioqueue.c grows rather than drops), so
   SDL_PutAudioStreamData never drops bytes. New at #529-A: a receipt LEDGER
   maps device-ingest bytes the host acknowledges consumed back to ORIGINAL
   application input bytes, so SDL_GetAudioStreamQueued reports exact
   original bytes not yet fully processed (for the universal src == open-spec
   case that is byte-for-byte the old ring+backlog number). A source-format
   change (SDL_SetAudioStreamFormat) converts FUTURE puts in C to the
   stream's immutable device-ingest spec before they enter the unchanged
   backlog/ring; a destination change is ignored without error, exactly as
   SDL specifies for the device-controlled side. The reported destination
   format is a read-only host query made at open (__sdl_audio_dst_query):
   the gucOS kernel sink is F32/stereo/48000, a standalone page reports its
   receiver's Web Audio graph format, and a sink-less host reports the
   requested spec — SDL's own dummy driver contract ("don't change reported
   device format"). A host that cannot answer makes open FAIL rather than
   invent a format.

   MEMORY streams (SDL_CreateAudioStream) are device-less converters:
   put source bytes, get converted bytes. Validation follows pinned SDL
   release-3.4.0: the five published formats, 1..8 channels, any positive
   rate. Format state is EPOCHED: every put snapshots the current src/dst
   specs into an immutable input extent (coalescing only with a same-spec,
   unflushed tail), SDL_SetAudioStreamFormat changes only the CURRENT specs,
   and queued bytes are always decoded/encoded with their captured specs —
   nothing queued is ever reinterpreted. Conversion is LAZY: only
   SDL_GetAudioStreamData converts, and it converts exactly what the caller's
   buffer holds (whole destination frames of the head epoch; it crosses into
   a later epoch only when at least one whole frame of that epoch fits), so
   no hidden converted buffers exist. SDL_GetAudioStreamAvailable is a pure
   size SIMULATION over the epoch list — allocation-free, mutation-free.
   The pipeline is float: pinned scalar sample codecs (release-3.4.0
   SDL_audiotypecvt.c bit-exact bodies), the pinned SDL default 1..8
   channel-conversion matrix, then persistent-phase linear-interpolation
   resampling with an exact integer-rational phase (output frame k of an
   epoch samples source position k*src_freq/dst_freq — chunk-boundary
   invariant by construction). A flush marks the tail epoch's end so its
   finite resampler tail (hold-last-frame extrapolation) becomes emittable;
   it is a generation marker, not EOF — a later put starts a new epoch.
   An epoch that can no longer grow (a newer epoch follows it) emits its
   tail the same way, so no queued byte is ever stranded. */

__import int __sdl_audio_dst_query(int dev, int sel);   /* sel 0=format 1=channels 2=freq; <=0 = cannot report */

/* -- format helpers ------------------------------------------------------ */

static int __au_fmt_bytes(int f) {
    switch (f) {
    case SDL_AUDIO_U8: case SDL_AUDIO_S8: return 1;
    case SDL_AUDIO_S16: return 2;
    case SDL_AUDIO_S32: case SDL_AUDIO_F32: return 4;
    }
    return 0;
}

static int __au_frame_bytes(const SDL_AudioSpec *s) {
    return __au_fmt_bytes(s->format) * s->channels;
}

static int __au_spec_eq(const SDL_AudioSpec *a, const SDL_AudioSpec *b) {
    return a->format == b->format && a->channels == b->channels && a->freq == b->freq;
}

/* Pinned MEMORY-side validation (SDL release-3.4.0 SDL_audiocvt.c
   SDL_IsSupportedAudioFormat / SDL_IsSupportedChannelCount / freq > 0):
   five formats, 1..8 channels, any positive int rate — deliberately no
   host-derived maximum. DEVICE transport limits (4000..192000 Hz, 1..2
   channels) are enforced by the host at open, never here. */
static bool __au_validate_side(const SDL_AudioSpec *s, int is_src) {
    if (!__au_fmt_bytes(s->format))
        return is_src ? SDL_InvalidParamError("src_spec->format") : SDL_InvalidParamError("dst_spec->format");
    if (s->channels < 1 || s->channels > 8)
        return is_src ? SDL_InvalidParamError("src_spec->channels") : SDL_InvalidParamError("dst_spec->channels");
    if (s->freq <= 0)
        return is_src ? SDL_InvalidParamError("src_spec->freq") : SDL_InvalidParamError("dst_spec->freq");
    return 1;
}

/* -- pinned scalar sample codecs (release-3.4.0 SDL_audiotypecvt.c) -------
   Decoders are the arithmetic equivalents of upstream's bit tricks (both
   are exact: the scales are powers of two). Encoders are upstream's scalar
   bodies verbatim — they round to nearest-even and saturate to the exact
   integer range, and their NaN/overflow behavior is pinned by construction.
   F32 destinations store the computed value unclamped, as upstream does. */

static float __au_dec_sample(const unsigned char *p, int fmt) {
    switch (fmt) {
    case SDL_AUDIO_F32: { float v; memcpy(&v, p, 4); return v; }
    case SDL_AUDIO_S32: { int v; memcpy(&v, p, 4); return (float)v * 0.0000000004656612873077392578125f; }
    case SDL_AUDIO_S16: { short v; memcpy(&v, p, 2); return (float)v * 0.000030517578125f; }
    case SDL_AUDIO_S8:  return (float)(signed char)p[0] * 0.0078125f;
    default:            return (float)((int)p[0] - 128) * 0.0078125f;   /* U8 */
    }
}

#define __AU_SIGNMASK(x) ((unsigned int)(0u - ((unsigned int)(x) >> 31)))

static void __au_enc_sample(unsigned char *p, int fmt, float v) {
    union { float f; unsigned int u; } x;
    unsigned int y, z;
    switch (fmt) {
    case SDL_AUDIO_F32:
        memcpy(p, &v, 4);
        return;
    case SDL_AUDIO_S32:
        x.f = v;
        y = x.u + 0x0F800000u;
        z = y - 0xCF000000u;
        z &= __AU_SIGNMASK(y ^ z);
        x.u = y - z;
        { int out = (int)x.f ^ (int)__AU_SIGNMASK(z); memcpy(p, &out, 4); }
        return;
    case SDL_AUDIO_S16:
        x.f = v + 384.0f;
        y = x.u - 0x43C00000u;
        z = 0x7FFFu - (y ^ __AU_SIGNMASK(y));
        y = y ^ (z & __AU_SIGNMASK(z));
        { short out = (short)(y & 0xFFFFu); memcpy(p, &out, 2); }
        return;
    case SDL_AUDIO_S8:
        x.f = v + 98304.0f;
        y = x.u - 0x47C00000u;
        z = 0x7Fu - (y ^ __AU_SIGNMASK(y));
        y = y ^ (z & __AU_SIGNMASK(z));
        p[0] = (unsigned char)(y & 0xFFu);
        return;
    default:   /* U8 */
        x.f = v + 98304.0f;
        y = x.u - 0x47C00000u;
        z = 0x7Fu - (y ^ __AU_SIGNMASK(y));
        y = (y ^ 0x80u) ^ (z & __AU_SIGNMASK(z));
        p[0] = (unsigned char)(y & 0xFFu);
        return;
    }
}

/* One frame of the pinned SDL default channel-conversion matrix
   (libsdl-org/SDL release-3.4.0 src/audio/SDL_audio_channel_converters.h,
   generated there from the FAudio matrix_defaults table; zlib license, see
   the provenance note above). Mechanically re-emitted as one per-frame
   switch of direct assignments — same coefficients, same ascending-source
   accumulation order, so values are bit-identical to the upstream scalar
   converters. A switch (not upstream's function-pointer table) keeps every
   case a direct call target: nothing here is address-taken, so an app that
   never converts channels links none of it. Identity counts (from == to)
   never reach this function. */
static void __au_chcvt_frame(int from, int to, const float *in, float *out) {
    switch (from * 16 + to) {
    case 18: /* Mono -> Stereo */
        out[0] = in[0];
        out[1] = in[0];
        return;
    case 19: /* Mono -> 21 */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        return;
    case 20: /* Mono -> Quad */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        out[3] = 0.0f;
        return;
    case 21: /* Mono -> 41 */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        return;
    case 22: /* Mono -> 51 */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        return;
    case 23: /* Mono -> 61 */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        return;
    case 24: /* Mono -> 71 */
        out[0] = in[0];
        out[1] = in[0];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 33: /* Stereo -> Mono */
        out[0] = (in[0] * 0.500000000f) + (in[1] * 0.500000000f);
        return;
    case 35: /* Stereo -> 21 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        return;
    case 36: /* Stereo -> Quad */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        return;
    case 37: /* Stereo -> 41 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        return;
    case 38: /* Stereo -> 51 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        return;
    case 39: /* Stereo -> 61 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        return;
    case 40: /* Stereo -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 49: /* 21 -> Mono */
        out[0] = (in[0] * 0.333333343f) + (in[1] * 0.333333343f) + (in[2] * 0.333333343f);
        return;
    case 50: /* 21 -> Stereo */
        out[0] = (in[0] * 0.800000012f) + (in[2] * 0.200000003f);
        out[1] = (in[1] * 0.800000012f) + (in[2] * 0.200000003f);
        return;
    case 52: /* 21 -> Quad */
        out[0] = (in[2] * 0.111111112f) + (in[0] * 0.888888896f);
        out[1] = (in[2] * 0.111111112f) + (in[1] * 0.888888896f);
        out[2] = (in[2] * 0.111111112f);
        out[3] = (in[2] * 0.111111112f);
        return;
    case 53: /* 21 -> 41 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = 0.0f;
        out[4] = 0.0f;
        return;
    case 54: /* 21 -> 51 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = 0.0f;
        out[5] = 0.0f;
        return;
    case 55: /* 21 -> 61 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        return;
    case 56: /* 21 -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = 0.0f;
        out[5] = 0.0f;
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 65: /* Quad -> Mono */
        out[0] = (in[0] * 0.250000000f) + (in[1] * 0.250000000f) + (in[2] * 0.250000000f) + (in[3] * 0.250000000f);
        return;
    case 66: /* Quad -> Stereo */
        out[0] = (in[0] * 0.421000004f) + (in[2] * 0.358999997f) + (in[3] * 0.219999999f);
        out[1] = (in[1] * 0.421000004f) + (in[2] * 0.219999999f) + (in[3] * 0.358999997f);
        return;
    case 67: /* Quad -> 21 */
        out[0] = (in[0] * 0.421000004f) + (in[2] * 0.358999997f) + (in[3] * 0.219999999f);
        out[1] = (in[1] * 0.421000004f) + (in[2] * 0.219999999f) + (in[3] * 0.358999997f);
        out[2] = 0.0f;
        return;
    case 69: /* Quad -> 41 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = in[3];
        return;
    case 70: /* Quad -> 51 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = in[2];
        out[5] = in[3];
        return;
    case 71: /* Quad -> 61 */
        out[0] = (in[0] * 0.939999998f);
        out[1] = (in[1] * 0.939999998f);
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = (in[3] * 0.500000000f) + (in[2] * 0.500000000f);
        out[5] = (in[2] * 0.796000004f);
        out[6] = (in[3] * 0.796000004f);
        return;
    case 72: /* Quad -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
        out[4] = in[2];
        out[5] = in[3];
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 81: /* 41 -> Mono */
        out[0] = (in[0] * 0.200000003f) + (in[1] * 0.200000003f) + (in[2] * 0.200000003f) + (in[3] * 0.200000003f) + (in[4] * 0.200000003f);
        return;
    case 82: /* 41 -> Stereo */
        out[0] = (in[0] * 0.374222219f) + (in[2] * 0.111111112f) + (in[3] * 0.319111109f) + (in[4] * 0.195555553f);
        out[1] = (in[1] * 0.374222219f) + (in[2] * 0.111111112f) + (in[3] * 0.195555553f) + (in[4] * 0.319111109f);
        return;
    case 83: /* 41 -> 21 */
        out[0] = (in[0] * 0.421000004f) + (in[3] * 0.358999997f) + (in[4] * 0.219999999f);
        out[1] = (in[1] * 0.421000004f) + (in[3] * 0.219999999f) + (in[4] * 0.358999997f);
        out[2] = in[2];
        return;
    case 84: /* 41 -> Quad */
        out[0] = (in[0] * 0.941176474f) + (in[2] * 0.058823530f);
        out[1] = (in[1] * 0.941176474f) + (in[2] * 0.058823530f);
        out[2] = (in[2] * 0.058823530f) + (in[3] * 0.941176474f);
        out[3] = (in[2] * 0.058823530f) + (in[4] * 0.941176474f);
        return;
    case 86: /* 41 -> 51 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = in[3];
        out[5] = in[4];
        return;
    case 87: /* 41 -> 61 */
        out[0] = (in[0] * 0.939999998f);
        out[1] = (in[1] * 0.939999998f);
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = (in[4] * 0.500000000f) + (in[3] * 0.500000000f);
        out[5] = (in[3] * 0.796000004f);
        out[6] = (in[4] * 0.796000004f);
        return;
    case 88: /* 41 -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = 0.0f;
        out[3] = in[2];
        out[4] = in[3];
        out[5] = in[4];
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 97: /* 51 -> Mono */
        out[0] = (in[0] * 0.166666672f) + (in[1] * 0.166666672f) + (in[2] * 0.166666672f) + (in[3] * 0.166666672f) + (in[4] * 0.166666672f) + (in[5] * 0.166666672f);
        return;
    case 98: /* 51 -> Stereo */
        out[0] = (in[0] * 0.294545442f) + (in[2] * 0.208181813f) + (in[3] * 0.090909094f) + (in[4] * 0.251818180f) + (in[5] * 0.154545456f);
        out[1] = (in[1] * 0.294545442f) + (in[2] * 0.208181813f) + (in[3] * 0.090909094f) + (in[4] * 0.154545456f) + (in[5] * 0.251818180f);
        return;
    case 99: /* 51 -> 21 */
        out[0] = (in[0] * 0.324000001f) + (in[2] * 0.229000002f) + (in[4] * 0.277000010f) + (in[5] * 0.170000002f);
        out[1] = (in[1] * 0.324000001f) + (in[2] * 0.229000002f) + (in[4] * 0.170000002f) + (in[5] * 0.277000010f);
        out[2] = in[3];
        return;
    case 100: /* 51 -> Quad */
        out[0] = (in[0] * 0.558095276f) + (in[2] * 0.394285709f) + (in[3] * 0.047619049f);
        out[1] = (in[1] * 0.558095276f) + (in[2] * 0.394285709f) + (in[3] * 0.047619049f);
        out[2] = (in[3] * 0.047619049f) + (in[4] * 0.558095276f);
        out[3] = (in[3] * 0.047619049f) + (in[5] * 0.558095276f);
        return;
    case 101: /* 51 -> 41 */
        out[0] = (in[0] * 0.586000025f) + (in[2] * 0.414000005f);
        out[1] = (in[1] * 0.586000025f) + (in[2] * 0.414000005f);
        out[2] = in[3];
        out[3] = (in[4] * 0.586000025f);
        out[4] = (in[5] * 0.586000025f);
        return;
    case 103: /* 51 -> 61 */
        out[0] = (in[0] * 0.939999998f);
        out[1] = (in[1] * 0.939999998f);
        out[2] = (in[2] * 0.939999998f);
        out[3] = in[3];
        out[4] = (in[5] * 0.500000000f) + (in[4] * 0.500000000f);
        out[5] = (in[4] * 0.796000004f);
        out[6] = (in[5] * 0.796000004f);
        return;
    case 104: /* 51 -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = in[3];
        out[4] = in[4];
        out[5] = in[5];
        out[6] = 0.0f;
        out[7] = 0.0f;
        return;
    case 113: /* 61 -> Mono */
        out[0] = (in[0] * 0.143142849f) + (in[1] * 0.143142849f) + (in[2] * 0.143142849f) + (in[3] * 0.142857149f) + (in[4] * 0.143142849f) + (in[5] * 0.143142849f) + (in[6] * 0.143142849f);
        return;
    case 114: /* 61 -> Stereo */
        out[0] = (in[0] * 0.247384623f) + (in[2] * 0.174461529f) + (in[3] * 0.076923080f) + (in[4] * 0.174461529f) + (in[5] * 0.226153851f) + (in[6] * 0.100615382f);
        out[1] = (in[1] * 0.247384623f) + (in[2] * 0.174461529f) + (in[3] * 0.076923080f) + (in[4] * 0.174461529f) + (in[5] * 0.100615382f) + (in[6] * 0.226153851f);
        return;
    case 115: /* 61 -> 21 */
        out[0] = (in[0] * 0.268000007f) + (in[2] * 0.188999996f) + (in[4] * 0.188999996f) + (in[5] * 0.245000005f) + (in[6] * 0.108999997f);
        out[1] = (in[1] * 0.268000007f) + (in[2] * 0.188999996f) + (in[4] * 0.188999996f) + (in[5] * 0.108999997f) + (in[6] * 0.245000005f);
        out[2] = in[3];
        return;
    case 116: /* 61 -> Quad */
        out[0] = (in[0] * 0.463679999f) + (in[2] * 0.327360004f) + (in[3] * 0.040000003f) + (in[5] * 0.168960005f);
        out[1] = (in[1] * 0.463679999f) + (in[2] * 0.327360004f) + (in[3] * 0.040000003f) + (in[6] * 0.168960005f);
        out[2] = (in[3] * 0.040000003f) + (in[4] * 0.327360004f) + (in[5] * 0.431039989f);
        out[3] = (in[3] * 0.040000003f) + (in[4] * 0.327360004f) + (in[6] * 0.431039989f);
        return;
    case 117: /* 61 -> 41 */
        out[0] = (in[0] * 0.483000010f) + (in[2] * 0.340999991f) + (in[5] * 0.175999999f);
        out[1] = (in[1] * 0.483000010f) + (in[2] * 0.340999991f) + (in[6] * 0.175999999f);
        out[2] = in[3];
        out[3] = (in[4] * 0.340999991f) + (in[5] * 0.449000001f);
        out[4] = (in[4] * 0.340999991f) + (in[6] * 0.449000001f);
        return;
    case 118: /* 61 -> 51 */
        out[0] = (in[0] * 0.611000001f) + (in[5] * 0.223000005f);
        out[1] = (in[1] * 0.611000001f) + (in[6] * 0.223000005f);
        out[2] = (in[2] * 0.611000001f);
        out[3] = in[3];
        out[4] = (in[4] * 0.432000011f) + (in[5] * 0.568000019f);
        out[5] = (in[4] * 0.432000011f) + (in[6] * 0.568000019f);
        return;
    case 120: /* 61 -> 71 */
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = in[3];
        out[4] = (in[4] * 0.707000017f);
        out[5] = (in[4] * 0.707000017f);
        out[6] = in[5];
        out[7] = in[6];
        return;
    case 129: /* 71 -> Mono */
        out[0] = (in[0] * 0.125125006f) + (in[1] * 0.125125006f) + (in[2] * 0.125125006f) + (in[3] * 0.125000000f) + (in[4] * 0.125125006f) + (in[5] * 0.125125006f) + (in[6] * 0.125125006f) + (in[7] * 0.125125006f);
        return;
    case 130: /* 71 -> Stereo */
        out[0] = (in[0] * 0.211866662f) + (in[2] * 0.150266662f) + (in[3] * 0.066666670f) + (in[4] * 0.181066677f) + (in[5] * 0.111066669f) + (in[6] * 0.194133341f) + (in[7] * 0.085866667f);
        out[1] = (in[1] * 0.211866662f) + (in[2] * 0.150266662f) + (in[3] * 0.066666670f) + (in[4] * 0.111066669f) + (in[5] * 0.181066677f) + (in[6] * 0.085866667f) + (in[7] * 0.194133341f);
        return;
    case 131: /* 71 -> 21 */
        out[0] = (in[0] * 0.226999998f) + (in[2] * 0.160999998f) + (in[4] * 0.194000006f) + (in[5] * 0.119000003f) + (in[6] * 0.208000004f) + (in[7] * 0.092000000f);
        out[1] = (in[1] * 0.226999998f) + (in[2] * 0.160999998f) + (in[4] * 0.119000003f) + (in[5] * 0.194000006f) + (in[6] * 0.092000000f) + (in[7] * 0.208000004f);
        out[2] = in[3];
        return;
    case 132: /* 71 -> Quad */
        out[0] = (in[0] * 0.466344833f) + (in[2] * 0.329241365f) + (in[3] * 0.034482758f) + (in[6] * 0.169931039f);
        out[1] = (in[1] * 0.466344833f) + (in[2] * 0.329241365f) + (in[3] * 0.034482758f) + (in[7] * 0.169931039f);
        out[2] = (in[3] * 0.034482758f) + (in[4] * 0.466344833f) + (in[6] * 0.433517247f);
        out[3] = (in[3] * 0.034482758f) + (in[5] * 0.466344833f) + (in[7] * 0.433517247f);
        return;
    case 133: /* 71 -> 41 */
        out[0] = (in[0] * 0.483000010f) + (in[2] * 0.340999991f) + (in[6] * 0.175999999f);
        out[1] = (in[1] * 0.483000010f) + (in[2] * 0.340999991f) + (in[7] * 0.175999999f);
        out[2] = in[3];
        out[3] = (in[4] * 0.483000010f) + (in[6] * 0.449000001f);
        out[4] = (in[5] * 0.483000010f) + (in[7] * 0.449000001f);
        return;
    case 134: /* 71 -> 51 */
        out[0] = (in[0] * 0.518000007f) + (in[6] * 0.188999996f);
        out[1] = (in[1] * 0.518000007f) + (in[7] * 0.188999996f);
        out[2] = (in[2] * 0.518000007f);
        out[3] = in[3];
        out[4] = (in[4] * 0.518000007f) + (in[6] * 0.481999993f);
        out[5] = (in[5] * 0.518000007f) + (in[7] * 0.481999993f);
        return;
    case 135: /* 71 -> 61 */
        out[0] = (in[0] * 0.541000009f);
        out[1] = (in[1] * 0.541000009f);
        out[2] = (in[2] * 0.541000009f);
        out[3] = in[3];
        out[4] = (in[4] * 0.287999988f) + (in[5] * 0.287999988f);
        out[5] = (in[4] * 0.458999991f) + (in[6] * 0.541000009f);
        out[6] = (in[5] * 0.458999991f) + (in[7] * 0.541000009f);
        return;
    }
}

/* -- immutable input extents (format epochs) ------------------------------
   One extent = one run of source bytes put under one (src, dst) spec pair.
   'base' is the source-frame index of data[0]: the consumed prefix a get no
   longer needs is compacted away (memmove) so a long-lived stream holds a
   bounded window, never the whole history. 'k' is the epoch's persistent
   resampler phase: output frame k samples exact source position
   k*src.freq/dst.freq (integer rational — no float drift, chunk-invariant).
   'retired' counts source frames conversion has fully consumed (the Queued
   instrument). 'flushed' marks the epoch's generation end. */
typedef struct __SDL_AuExtent {
    struct __SDL_AuExtent *next;
    SDL_AudioSpec src, dst;
    unsigned char *data;
    long long cap;            /* allocated bytes */
    long long frames;         /* whole source frames present (epoch total so far) */
    long long base;           /* source-frame index of data[0] */
    long long k;              /* destination frames already emitted */
    long long phase;          /* source position numerator, denominator dst.freq */
    long long retired;        /* source frames retired (no longer needed) */
    int flushed;
    int input_map[8],has_input_map;
    const void *external;
    int external_len;
    SDL_AudioStreamDataCompleteCallback complete;
    void *complete_userdata;
} __SDL_AuExtent;

/* DEVICE receipt ledger: one node per conversion epoch, FIFO. Identity
   nodes (src == device-ingest spec, the pre-#529 universal case) are
   byte-granular: one acknowledged device byte retires one original byte.
   Conversion nodes are frame-granular: after the host acknowledges 'a'
   device frames of this epoch, source frames below floor(a*src_freq/
   dev_freq) can never be needed again (the interpolation lookahead stays
   charged until its last dependent device frame is acknowledged), and a
   final (flushed) node retires everything at full acknowledgement. No
   fractional original frame is ever retired from a conversion node. */
typedef struct __SDL_AuLedger {
    struct __SDL_AuLedger *next;
    int identity;
    int closed;               /* no longer the growing tail */
    int final;                /* flush tail emitted: full ack retires all */
    int src_fb, dev_fb;
    long long src_freq, dev_freq;
    long long dev_bytes;      /* device bytes recorded (identity: == orig bytes) */
    long long acked_bytes;
    long long orig_frames;    /* conversion nodes: source frames recorded */
    long long dev_frames;     /* conversion nodes: device frames recorded */
    long long retired_frames;
} __SDL_AuLedger;

/* Extent/ledger allocation counter: a strictly observational
   SDL_GetAudioStreamAvailable must never move it (the Available oracle's
   fingerprint reads it through __sdl_audiostream_fp). */
static int __au_alloc_count;

/* Deterministic allocation-failure injection (review finding 2 tests).
   Production-unreachable: nothing in this file or any header calls the arm
   function; a test declares it 'extern' and arms the Nth allocation from
   now to fail. Every fallible allocation in the audio engine routes through
   these seams, and every put pre-reserves through them BEFORE any
   observable or internal mutation, so an armed failure proves atomicity. */
static int __au_fail_after;
int __sdl_audiostream_failalloc(int nth) { __au_fail_after = nth; return 0; }
static int __au_fail_hit(void) {
    if (__au_fail_after > 0 && --__au_fail_after == 0) return 1;
    return 0;
}
static void *__au_malloc(size_t n) { return __au_fail_hit() ? NULL : malloc(n); }
static void *__au_calloc(size_t n, size_t m) { return __au_fail_hit() ? NULL : calloc(n, m); }
static void *__au_realloc(void *p, size_t n) { return __au_fail_hit() ? NULL : realloc(p, n); }

struct SDL_AudioStream {
    pthread_mutex_t lock;
    SDL_PropertiesID properties;
    SDL_AudioStreamCallback get_callback,put_callback;
    void *get_userdata,*put_userdata;
    int callback_active;
    float gain,ratio;
    int output_map[8],has_output_map;
    int paused;
    int *shared_ring,shared_capacity,worker_started,worker_quit;
    pthread_t worker;
    int input_map[8],has_input_map;
    int kind;                 /* 0 = MEMORY, 1 = DEVICE */
    int has_src, has_dst;
    SDL_AudioSpec src, dst;   /* current configuration (DEVICE dst = host-reported sink) */
    __SDL_AuExtent *head, *tail;

    /* DEVICE only */
    int dev,public_dev;
    SDL_AudioSpec ingest;     /* immutable device-ingest spec (the open spec) */
    unsigned char *backlog;   /* converted device-ingest bytes the ring hasn't accepted */
    int backlog_len;
    int backlog_cap;
    __SDL_AuLedger *led_head, *led_tail;
    long long submitted_dev;             /* cum device bytes accepted by the host ring */
    long long last_host_queued;          /* reconcile baseline pair */
    long long submitted_at_last_query;
    long long ack_carry;                 /* sub-frame acknowledged bytes carried to the next reconcile */
    long long put_orig;                  /* cum original bytes put */
    long long retired_orig;              /* cum original bytes retired */
};

__import int __sdl_open_audio_device_shared(int,int,int,void *,int,int *);
__import int __sdl_queue_audio_shared(void *,int,const void *,int,int);
__import void __sdl_close_audio_device_shared(int);
__import int __sdl_clear_audio_shared(int);
static int __au_upgrade(SDL_AudioStream *s);
static bool __au_dev_finalize_epoch(SDL_AudioStream *s);
static int __au_queued(SDL_AudioStream *s){return s->shared_ring?__builtin(atomic_load32,s->shared_ring+1):__sdl_get_queued_audio_size(s->dev);}
static int __au_queue(SDL_AudioStream *s,const void *p,int n){return s->shared_ring?__sdl_queue_audio_shared(s->shared_ring,s->shared_capacity,p,n,__au_frame_bytes(&s->ingest)):__sdl_queue_audio(s->dev,p,n);}
static void __au_init_stream(SDL_AudioStream *s){pthread_mutexattr_t a;pthread_mutexattr_init(&a);pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);pthread_mutex_init(&s->lock,&a);pthread_mutexattr_destroy(&a);s->gain=1.0f;s->ratio=1.0f;s->paused=1;}
bool SDL_LockAudioStream(SDL_AudioStream *s){if(!s)return SDL_InvalidParamError("stream");return pthread_mutex_lock(&s->lock)==0;}
bool SDL_UnlockAudioStream(SDL_AudioStream *s){if(!s)return SDL_InvalidParamError("stream");return pthread_mutex_unlock(&s->lock)==0;}
SDL_PropertiesID SDL_GetAudioStreamProperties(SDL_AudioStream *s){if(!SDL_LockAudioStream(s))return 0;if(!s->properties)s->properties=SDL_CreateProperties();SDL_PropertiesID p=s->properties;SDL_UnlockAudioStream(s);return p;}
SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream *s){if(!s){SDL_InvalidParamError("stream");return 0;}return s->kind==1?(SDL_AudioDeviceID)s->public_dev:0;}
bool SDL_SetAudioStreamGetCallback(SDL_AudioStream *s,SDL_AudioStreamCallback callback,void *userdata){if(!SDL_LockAudioStream(s))return 0;if(s->kind==1&&callback&&!__au_upgrade(s)){SDL_UnlockAudioStream(s);return 0;}s->get_callback=callback;s->get_userdata=userdata;SDL_UnlockAudioStream(s);return 1;}
bool SDL_SetAudioStreamPutCallback(SDL_AudioStream *s,SDL_AudioStreamCallback callback,void *userdata){if(!SDL_LockAudioStream(s))return 0;s->put_callback=callback;s->put_userdata=userdata;SDL_UnlockAudioStream(s);return 1;}
bool SDL_SetAudioStreamGain(SDL_AudioStream *s,float gain){if(!s)return SDL_InvalidParamError("stream");if(!(gain>=0.0f)||!isfinite(gain))return SDL_InvalidParamError("gain");SDL_LockAudioStream(s);s->gain=gain;SDL_UnlockAudioStream(s);return 1;}
float SDL_GetAudioStreamGain(SDL_AudioStream *s){if(!SDL_LockAudioStream(s))return -1.0f;float gain=s->gain;SDL_UnlockAudioStream(s);return gain;}
float SDL_GetAudioStreamFrequencyRatio(SDL_AudioStream *s){if(!SDL_LockAudioStream(s))return 0.0f;float ratio=s->ratio;SDL_UnlockAudioStream(s);return ratio;}
bool SDL_SetAudioStreamInputChannelMap(SDL_AudioStream *s,const int *map,int count){if(!SDL_LockAudioStream(s))return 0;if(count!=s->src.channels||count<1||count>8){SDL_UnlockAudioStream(s);return SDL_SetError("Wrong number of input channels");}if(map)for(int i=0;i<count;i++)if(map[i]<-1||map[i]>=count){SDL_UnlockAudioStream(s);return SDL_SetError("Invalid input channel map");}if(s->kind==1&&!__au_dev_finalize_epoch(s)){SDL_UnlockAudioStream(s);return 0;}if(s->tail)s->tail->flushed=1;s->has_input_map=map!=NULL;if(map)memcpy(s->input_map,map,count*sizeof(int));SDL_UnlockAudioStream(s);return 1;}
bool SDL_SetAudioStreamOutputChannelMap(SDL_AudioStream *s,const int *map,int count){if(!SDL_LockAudioStream(s))return 0;if(count!=s->dst.channels||count<1||count>8){SDL_UnlockAudioStream(s);return SDL_SetError("Wrong number of output channels");}if(s->kind==1&&count!=s->ingest.channels){SDL_UnlockAudioStream(s);return SDL_SetError("Device channel mapping requires matching ingest and sink channel counts");}if(map)for(int i=0;i<count;i++)if(map[i]<-1||map[i]>=count){SDL_UnlockAudioStream(s);return SDL_SetError("Invalid output channel mapping");}s->has_output_map=map!=NULL;if(map)memcpy(s->output_map,map,count*sizeof(int));SDL_UnlockAudioStream(s);return 1;}
int *SDL_GetAudioStreamOutputChannelMap(SDL_AudioStream *s,int *count){if(count)*count=0;if(!SDL_LockAudioStream(s))return NULL;int channels=s->dst.channels;int *map=channels?malloc(channels*sizeof(int)):NULL;if(map){for(int i=0;i<channels;i++)map[i]=s->has_output_map?s->output_map[i]:i;if(count)*count=channels;}else SDL_SetError("No output channel map or out of memory");SDL_UnlockAudioStream(s);return map;}
int *SDL_GetAudioStreamInputChannelMap(SDL_AudioStream *s,int *count){if(count)*count=0;if(!SDL_LockAudioStream(s))return NULL;int channels=s->src.channels;int *map=channels?malloc(channels*sizeof(int)):NULL;if(map){for(int i=0;i<channels;i++)map[i]=s->has_input_map?s->input_map[i]:i;if(count)*count=channels;}else SDL_SetError("No input channel map or out of memory");SDL_UnlockAudioStream(s);return map;}
bool SDL_AudioStreamDevicePaused(SDL_AudioStream *s){if(!SDL_LockAudioStream(s))return 1;if(s->kind!=1){SDL_UnlockAudioStream(s);SDL_SetError("Stream is not bound to a device");return 1;}bool paused=s->paused;SDL_UnlockAudioStream(s);return paused;}
static long long __au_rate(const SDL_AudioStream *s,const __SDL_AuExtent *e){long long rate=(long long)((float)e->src.freq*s->ratio);return rate>0?rate:1;}
static long long __au_emit_from(long long N,long long sf,long long df,long long phase,int final){if(N<=0)return 0;long long limit=(final?N:N-1)*df;if(final){if(phase>=limit)return 0;return (limit-phase+sf-1)/sf;}if(phase>limit)return 0;return (limit-phase)/sf+1;}

/* -- epoch arithmetic ------------------------------------------------------
   For an epoch with N source frames at rates sf -> df:
   output frame k samples source position pos = k*sf/df: left index
   i = floor(pos), fraction r/df with r = (k*sf) % df. Without a flush the
   epoch holds back frames that would need a source sample that may still
   arrive (r == 0 needs i < N; r > 0 needs i+1 < N), giving
   floor((N-1)*df/sf) + 1 emittable frames. A final epoch extrapolates by
   holding the last source frame, giving ceil(N*df/sf). All in 64-bit:
   N and df are < 2^31 so N*df < 2^62. */

static long long __au_emit_total(long long N, long long sf, long long df, int final) {
    if (N <= 0) return 0;
    if (final) return (N * df + sf - 1) / sf;
    return (N - 1) * df / sf + 1;
}

/* A non-tail epoch can never grow (puts coalesce only with the tail), so it
   behaves as flushed — otherwise its resampler tail would be stranded. */
static int __au_contiguous(const __SDL_AuExtent *e){const __SDL_AuExtent *n=e->next;return n&&!e->flushed&&__au_spec_eq(&e->src,&n->src)&&__au_spec_eq(&e->dst,&n->dst)&&e->has_input_map==n->has_input_map&&(!e->has_input_map||!memcmp(e->input_map,n->input_map,sizeof(e->input_map)));}
static void __au_carry_phase(__SDL_AuExtent *e){if(__au_contiguous(e))e->next->phase=e->phase-(e->base+e->frames)*e->dst.freq;}
static int __au_extent_final(const SDL_AudioStream *s, const __SDL_AuExtent *e) {
    (void)s;
    return e->flushed || e->next != NULL;
}

/* Decode source frame 'idx' (absolute epoch index) of 'e' into out[ch],
   clamping to the last frame for the final-tail extrapolation. */
static void __au_dec_frame(SDL_AudioStream *s,const __SDL_AuExtent *e, long long idx, float *out) {
    int sb = __au_fmt_bytes(e->src.format);
    int fb = sb * e->src.channels;
    const unsigned char *p;
    int c;
    if(idx>=e->base+e->frames&&__au_contiguous(e)){__au_dec_frame(s,e->next,idx-e->base-e->frames,out);return;}
    if (idx > e->base + e->frames - 1) idx = e->base + e->frames - 1;
    p = e->data + (long long)(idx - e->base) * fb;
    for (c = 0; c < e->src.channels; c++)
        out[c] = e->has_input_map?(e->input_map[c]<0?0.0f:__au_dec_sample(p+(long long)e->input_map[c]*sb,e->src.format)):__au_dec_sample(p + (long long)c * sb, e->src.format);
}

/* Convert 'n' destination frames from epoch 'e' into 'out' (contiguous
   dst-spec frames), advancing the epoch phase. The caller guarantees
   n <= emittable. Pipeline per frame: decode both source frames, apply the
   pinned channel matrix, then linear-interpolate at the exact rational
   fraction. Same-spec fast path is a straight memcpy. */
static void __au_convert(SDL_AudioStream *s,__SDL_AuExtent *e, long long n, unsigned char *out) {
    long long sf = __au_rate(s,e), df = e->dst.freq;
    int dch = e->dst.channels, sch = e->src.channels;
    int dsb = __au_fmt_bytes(e->dst.format);
    int dfb = dsb * dch;
    long long j;
    if (e->src.format == e->dst.format && sch == dch && sf == df && e->phase%df==0 && s->gain==1.0f && !s->has_output_map && !e->has_input_map) {
        int sfb = __au_frame_bytes(&e->src);
        memcpy(out, e->data + (e->phase/df - e->base) * sfb, (size_t)(n * sfb));
        e->k += n;
        e->phase += n*sf;
    } else {
        for (j = 0; j < n; j++, out += dfb) {
            long long num = e->phase;
            long long i = num / df, r = num % df;
            float a[8], b[8], ca[8], cb[8];
            int c;
            __au_dec_frame(s,e, i, a);
            if (sch == dch) {
                for (c = 0; c < dch; c++) ca[c] = a[c];
            } else {
                __au_chcvt_frame(sch, dch, a, ca);
            }
            if (r != 0) {
                float t = (float)((double)r / (double)df);
                __au_dec_frame(s,e, i + 1, b);
                if (sch == dch) {
                    for (c = 0; c < dch; c++) cb[c] = b[c];
                } else {
                    __au_chcvt_frame(sch, dch, b, cb);
                }
                for (c = 0; c < dch; c++) ca[c] = ca[c] + (cb[c] - ca[c]) * t;
            }
            for (c = 0; c < dch; c++)
                __au_enc_sample(out + (long long)c * dsb, e->dst.format, s->gain*(s->has_output_map?(s->output_map[c]<0||s->output_map[c]>=dch?0.0f:ca[s->output_map[c]]):ca[c]));
            e->k++;
            e->phase+=sf;
        }
    }
    /* Retire the source prefix conversion can never need again, and compact
       it out of the buffer once it is worth a memmove. */
    {
        long long left = e->phase / df;
        int sfb = __au_frame_bytes(&e->src);
        if (left > e->frames + e->base) left = e->frames + e->base;
        if (left > e->retired) e->retired = left;
        if (!e->external && (e->retired - e->base) * sfb >= 65536) {
            long long dropf = e->retired - e->base;
            memmove(e->data, e->data + dropf * sfb, (size_t)((e->frames - dropf) * sfb));
            e->frames -= dropf;
            e->base += dropf;
        }
    }
}

static void __au_free_extent(__SDL_AuExtent *e){if(e->external){if(e->complete)e->complete(e->complete_userdata,e->external,e->external_len);}else free(e->data);free(e);}
static void __au_free_extents(SDL_AudioStream *s) {
    __SDL_AuExtent *e = s->head;
    s->head = s->tail = NULL;
    while (e) { __SDL_AuExtent *n = e->next; __au_free_extent(e); e = n; }
}

static void __au_free_ledger(SDL_AudioStream *s) {
    __SDL_AuLedger *l = s->led_head;
    while (l) { __SDL_AuLedger *n = l->next; free(l); l = n; }
    s->led_head = s->led_tail = NULL;
}

/* Reserve, then commit (review finding 2: a put must complete every
   fallible allocation BEFORE any observable or internal mutation, so a
   false return means zero bytes were accepted anywhere). Reserve either
   grows the coalescible tail's capacity in place (capacity is not state —
   no accounting word, offset, or fingerprint field changes; only committed
   reservations bump the allocation counter, via *allocs) or builds a
   DETACHED extent already carrying the copied bytes. Commit links/appends
   and cannot fail. */
static int __au_extent_coalesces(const SDL_AudioStream *s, const SDL_AudioSpec *dst) {
    const __SDL_AuExtent *e = s->tail;
    return e && !e->external && !e->flushed && __au_spec_eq(&e->src, &s->src) && __au_spec_eq(&e->dst, dst);
}

static bool __au_extent_reserve(SDL_AudioStream *s, const SDL_AudioSpec *dst,
                                const unsigned char *bytes, int len,
                                __SDL_AuExtent **detached, int *allocs) {
    int sfb = __au_frame_bytes(&s->src);
    __SDL_AuExtent *e;
    *detached = NULL;
    if (__au_extent_coalesces(s, dst)) {
        e = s->tail;
        {
            long long need = (e->frames * (long long)sfb) + len;
            if (need > e->cap) {
                long long ncap = e->cap ? e->cap : 4096;
                unsigned char *nd;
                while (ncap < need) ncap *= 2;
                nd = (unsigned char *)__au_realloc(e->data, (size_t)ncap);
                if (!nd) return SDL_SetError("Out of memory");
                (*allocs)++;
                e->data = nd;
                e->cap = ncap;
            }
        }
        return 1;
    }
    e = (__SDL_AuExtent *)__au_malloc(sizeof(__SDL_AuExtent));
    if (!e) return SDL_SetError("Out of memory");
    e->external=NULL;e->complete=NULL;e->complete_userdata=NULL;e->external_len=0;
    e->data = (unsigned char *)__au_malloc((size_t)len);
    if (!e->data) { free(e); return SDL_SetError("Out of memory"); }
    *allocs += 2;
    memcpy(e->data, bytes, (size_t)len);
    e->cap = len;
    e->src = s->src;
    e->has_input_map=s->has_input_map;memcpy(e->input_map,s->input_map,sizeof(e->input_map));
    e->dst = *dst;
    e->frames = 0;            /* counted at commit */
    e->base = 0;
    e->k = 0;
    e->phase = 0;
    e->retired = 0;
    e->flushed = 0;
    e->next = NULL;
    *detached = e;
    return 1;
}

static void __au_extent_commit(SDL_AudioStream *s, __SDL_AuExtent *detached,
                               const unsigned char *bytes, int len) {
    int sfb = __au_frame_bytes(&s->src);
    if (detached) {
        detached->frames = len / sfb;
        if (s->tail) s->tail->next = detached; else s->head = detached;
        s->tail = detached;
        return;
    }
    memcpy(s->tail->data + s->tail->frames * sfb, bytes, (size_t)len);
    s->tail->frames += len / sfb;
}

/* -- DEVICE receipt ledger + reconcile ------------------------------------ */

/* Link a pre-allocated ledger node as the new tail, closing the old tail.
   Allocation happens at the put's reserve step; linking cannot fail. */
static void __au_led_link(SDL_AudioStream *s, __SDL_AuLedger *L) {
    if (s->led_tail) { s->led_tail->closed = 1; s->led_tail->next = L; } else s->led_head = L;
    s->led_tail = L;
}

/* One synchronous host acknowledgement read: consumed = baseline + newly
   submitted - currently queued. Acknowledged bytes walk the ledger FIFO;
   identity nodes retire byte-for-byte, conversion nodes retire only whole
   original frames at floor(acked_frames * src_freq / dev_freq) (all frames
   at full acknowledgement of a final node). Sub-frame acknowledgement (a
   host quirk — both shipped consumers drain whole frames) carries to the
   next reconcile rather than over-retiring. */
static void __au_dev_reconcile(SDL_AudioStream *s) {
    long long cur = __au_queued(s);
    long long consumed;
    if (cur < 0) cur = 0;
    consumed = s->last_host_queued + (s->submitted_dev - s->submitted_at_last_query) - cur;
    s->last_host_queued = cur;
    s->submitted_at_last_query = s->submitted_dev;
    if (consumed <= 0) return;
    consumed += s->ack_carry;
    s->ack_carry = 0;
    while (consumed > 0 && s->led_head) {
        __SDL_AuLedger *L = s->led_head;
        long long avail, take;
        if (L->identity) {
            avail = L->dev_bytes - L->acked_bytes;
            take = consumed < avail ? consumed : avail;
            L->acked_bytes += take;
            s->retired_orig += take;
            consumed -= take;
        } else {
            long long recorded = L->dev_frames * L->dev_fb;
            long long af, mark, delta;
            avail = recorded - L->acked_bytes;
            take = consumed < avail ? consumed : avail;
            L->acked_bytes += take;
            consumed -= take;
            af = L->acked_bytes / L->dev_fb;
            if (L->final && L->closed && L->acked_bytes == recorded) {
                mark = L->orig_frames;
            } else {
                mark = af * L->src_freq / L->dev_freq;
                if (mark > L->orig_frames) mark = L->orig_frames;
            }
            delta = mark - L->retired_frames;
            if (delta > 0) {
                L->retired_frames = mark;
                s->retired_orig += delta * L->src_fb;
            }
        }
        if (L->closed && L->acked_bytes == (L->identity ? L->dev_bytes : L->dev_frames * L->dev_fb)) {
            s->led_head = L->next;
            if (!s->led_head) s->led_tail = NULL;
            free(L);
            continue;
        }
        break;   /* an open tail (or a partial ack) ends the walk */
    }
    if (consumed > 0) s->ack_carry = consumed;
}

/* Push as much of the backlog into the host ring as it will currently
   accept, then compact the remainder to the front. __sdl_queue_audio returns
   the number of bytes the ring actually accepted; those count as SUBMITTED
   for the acknowledgement baseline. */
static void __sdl_stream_pump(SDL_AudioStream *s) {
    int accepted;
    if (s->backlog_len <= 0) return;
    accepted = __au_queue(s, s->backlog, s->backlog_len);
    if (accepted <= 0) return;
    s->submitted_dev += accepted;
    if (accepted >= s->backlog_len) { s->backlog_len = 0; return; }
    memmove(s->backlog, s->backlog + accepted, (size_t)(s->backlog_len - accepted));
    s->backlog_len -= accepted;
}

static bool __au_backlog_reserve(SDL_AudioStream *s, int add, int *allocs) {
    if (add > 0x7FFFFFFF - s->backlog_len) return SDL_SetError("audio backlog overflow");
    if (s->backlog_len + add > s->backlog_cap) {
        int ncap = s->backlog_cap ? s->backlog_cap : 65536;
        unsigned char *nb;
        while (ncap < s->backlog_len + add) {
            if (ncap > 0x40000000) { ncap = 0x7FFFFFFF; break; }
            ncap *= 2;
        }
        nb = (unsigned char *)__au_realloc(s->backlog, (size_t)ncap);
        if (!nb) return SDL_SetError("Out of memory");
        (*allocs)++;
        s->backlog = nb;
        s->backlog_cap = ncap;
    }
    return 1;
}

/* Convert 'n' device-ingest frames from the pending epoch into the backlog
   and record them on the ledger tail. */
static bool __au_dev_emit(SDL_AudioStream *s, __SDL_AuExtent *e, long long n) {
    int ifb = __au_frame_bytes(&s->ingest);
    int allocs = 0;
    if (n <= 0) return 1;
    if (n > (long long)(0x7FFFFFFF) / ifb) return SDL_SetError("audio backlog overflow");
    if (!__au_backlog_reserve(s, (int)(n * ifb), &allocs)) return 0;
    __au_alloc_count += allocs;
    __au_convert(s,e, n, s->backlog + s->backlog_len);
    s->backlog_len += (int)(n * ifb);
    s->led_tail->dev_frames += n;
    return 1;
}

/* Finalize the pending DEVICE source epoch: emit every remaining complete
   frame plus the finite resampler tail, mark its ledger node final, and
   free the extent. Runs at flush and at a source-format transition; a
   second flush with no new put finds nothing and emits nothing. */
static bool __au_dev_finalize_epoch(SDL_AudioStream *s) {
    __SDL_AuExtent *e = s->head;
    if (e) {
        long long total = e->k+__au_emit_from(e->base+e->frames,__au_rate(s,e),e->dst.freq,e->phase,1);
        if (!__au_dev_emit(s, e, total - e->k)) return 0;
        s->led_tail->final = 1;
        s->led_tail->closed = 1;
        s->head = s->tail = NULL;
        __au_free_extent(e);
    }
    return 1;
}

bool SDL_SetAudioStreamFrequencyRatio(SDL_AudioStream *s,float ratio){if(!s)return SDL_InvalidParamError("stream");if(!(ratio>=0.01f&&ratio<=100.0f))return SDL_InvalidParamError("ratio");SDL_LockAudioStream(s);if(s->kind==1&&s->ratio!=ratio&&!__au_dev_finalize_epoch(s)){SDL_UnlockAudioStream(s);return 0;}s->ratio=ratio;SDL_UnlockAudioStream(s);return 1;}

/* -- public API ------------------------------------------------------------ */

SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *src_spec,
                                       const SDL_AudioSpec *dst_spec) {
    SDL_AudioStream *s;
    if (src_spec && !__au_validate_side(src_spec, 1)) return NULL;
    if (dst_spec && !__au_validate_side(dst_spec, 0)) return NULL;
    s = (SDL_AudioStream *)__au_calloc(1, sizeof(SDL_AudioStream));
    if (!s) { SDL_SetError("Out of memory"); return NULL; }
    __au_init_stream(s);
    if (src_spec) { s->src = *src_spec; s->has_src = 1; }
    if (dst_spec) { s->dst = *dst_spec; s->has_dst = 1; }
    return s;
}

static bool __locked_SDL_SetAudioStreamFormat(SDL_AudioStream *stream,
                              const SDL_AudioSpec *src_spec,
                              const SDL_AudioSpec *dst_spec) {
    if (!stream) return SDL_InvalidParamError("stream");
    /* Validate every non-NULL replacement BEFORE changing either side, so
       the operation is atomic — including a destination a device stream
       will then ignore (upstream validates first, ignores second). */
    if (src_spec && !__au_validate_side(src_spec, 1)) return 0;
    if (dst_spec && !__au_validate_side(dst_spec, 0)) return 0;
    if (stream->kind == 1) {
        dst_spec = NULL;   /* device-controlled side: quietly not changed */
        if (src_spec && !__au_spec_eq(src_spec, &stream->src)) {
            /* Identity mode may hold a dangling partial ingest frame (device
               puts are byte-oriented); changing the source spec would shift
               every later frame boundary in the byte stream. Refuse — the
               bytes are the app's, never reinterpreted. */
            if (!stream->head && stream->led_tail && stream->led_tail->identity &&
                (stream->led_tail->dev_bytes % __au_frame_bytes(&stream->ingest)) != 0)
                return SDL_SetError("SDL_SetAudioStreamFormat: a partial device frame is pending; complete the frame before changing the source format");
            if (!__au_dev_finalize_epoch(stream)) return 0;
            if (stream->led_tail && !stream->led_tail->closed) stream->led_tail->closed = 1;
            if(src_spec->channels!=stream->src.channels)stream->has_input_map=0;stream->src = *src_spec;
            return 1;
        }
        return 1;
    }
    if (src_spec) { stream->src = *src_spec; stream->has_src = 1; }
    if (dst_spec) { if(dst_spec->channels!=stream->dst.channels)stream->has_output_map=0;stream->dst = *dst_spec; stream->has_dst = 1; }
    return 1;
}
bool SDL_SetAudioStreamFormat(SDL_AudioStream *stream,
                              const SDL_AudioSpec *src_spec,
                              const SDL_AudioSpec *dst_spec){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_SetAudioStreamFormat(stream,src_spec,dst_spec);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_GetAudioStreamFormat(SDL_AudioStream *stream,
                              SDL_AudioSpec *src_spec,
                              SDL_AudioSpec *dst_spec) {
    if (!stream) {
        if (src_spec) memset(src_spec, 0, sizeof *src_spec);
        if (dst_spec) memset(dst_spec, 0, sizeof *dst_spec);
        return SDL_InvalidParamError("stream");
    }
    /* Fill each requested side with its CURRENT configuration (a zeroed spec
       for an unset MEMORY side — pinned upstream shape), then report the
       unset side as the error, source first, as upstream does. */
    if (src_spec) {
        if (stream->has_src) *src_spec = stream->src;
        else memset(src_spec, 0, sizeof *src_spec);
    }
    if (dst_spec) {
        if (stream->has_dst) *dst_spec = stream->dst;
        else memset(dst_spec, 0, sizeof *dst_spec);
    }
    if (src_spec && !stream->has_src) return SDL_SetError("Stream has no source format");
    if (dst_spec && !stream->has_dst) return SDL_SetError("Stream has no destination format");
    return 1;
}
bool SDL_GetAudioStreamFormat(SDL_AudioStream *stream,
                              SDL_AudioSpec *src_spec,
                              SDL_AudioSpec *dst_spec){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_GetAudioStreamFormat(stream,src_spec,dst_spec);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len) {
    if (!stream) return SDL_InvalidParamError("stream");
    if (!buf) return SDL_InvalidParamError("buf");
    if (len < 0) return SDL_InvalidParamError("len");
    if (len == 0) return 1;   /* nothing to do — and no epoch is created */
    /* ATOMICITY (review finding 2): every fallible allocation/reservation in
       each branch below completes BEFORE any host submission, backlog byte,
       ledger/original-byte mutation, or converter-state change. A false
       return therefore accepted zero caller bytes and left every observable
       and internal word — the allocation counter included — unchanged. */
    if (stream->kind == 0) {
        __SDL_AuExtent *det;
        int allocs = 0;
        if (!stream->has_src) return SDL_SetError("Stream has no source format");
        if (!stream->has_dst) return SDL_SetError("Stream has no destination format");
        if (len % __au_frame_bytes(&stream->src) != 0)
            return SDL_SetError("Can't add partial sample frames");
        if (!__au_extent_reserve(stream, &stream->dst, (const unsigned char *)buf, len, &det, &allocs))
            return 0;
        __au_extent_commit(stream, det, (const unsigned char *)buf, len);
        __au_alloc_count += allocs;
        return 1;
    }
    /* DEVICE */
    if (stream->head == NULL && __au_spec_eq(&stream->src, &stream->ingest) && stream->gain==1.0f && stream->ratio==1.0f && !stream->has_output_map && !stream->has_input_map) {
        /* Identity fast path — the pre-#529 behavior, byte-for-byte. Reserve
           first: a ledger tail if none is open, and worst-case backlog
           capacity (the ring may accept nothing). Then drain any backlog so
           the ring is as empty as possible; only write the new data straight
           to the ring if the backlog is empty (FIFO). */
        __SDL_AuLedger *pre = NULL;
        int allocs = 0;
        int accepted = 0;
        if (!(stream->led_tail && stream->led_tail->identity && !stream->led_tail->closed)) {
            pre = (__SDL_AuLedger *)__au_calloc(1, sizeof(__SDL_AuLedger));
            if (!pre) return SDL_SetError("Out of memory");
            pre->identity = 1;
        }
        if (!__au_backlog_reserve(stream, len, &allocs)) { free(pre); return 0; }
        /* commit — nothing below can fail */
        __au_alloc_count += allocs;
        __sdl_stream_pump(stream);
        if (stream->backlog_len == 0) {
            accepted = __au_queue(stream, buf, len);
            if (accepted < 0) accepted = 0;
            stream->submitted_dev += accepted;
        }
        if (pre) { __au_led_link(stream, pre); __au_alloc_count++; }
        stream->led_tail->dev_bytes += len;
        stream->put_orig += len;
        if (accepted < len) {
            memcpy(stream->backlog + stream->backlog_len,
                   (const unsigned char *)buf + accepted, (size_t)(len - accepted));
            stream->backlog_len += len - accepted;
        }
        return 1;
    }
    if(stream->head&&stream->head->external&&!__au_dev_finalize_epoch(stream))return 0;
    /* Converted mode: the current source spec differs from the immutable
       device-ingest spec (or an epoch is already pending). Puts are
       frame-aligned like MEMORY puts; complete frames convert eagerly
       (interpolation lookahead held until flush), converted ingest bytes
       join the unchanged backlog/ring, and the ledger records the epoch. */
    {
        int sfb = __au_frame_bytes(&stream->src);
        int ifb = __au_frame_bytes(&stream->ingest);
        __SDL_AuExtent *ehead = stream->head;
        __SDL_AuLedger *node = NULL;
        __SDL_AuExtent *det = NULL;
        int allocs = 0;
        long long total_after, n_emit;
        if (len % sfb != 0)
            return SDL_SetError("Can't add partial sample frames");
        /* the eager emit this put will produce, reserved up front */
        total_after = (ehead ? ehead->base + ehead->frames : 0) + len / sfb;
        n_emit = __au_emit_from(total_after,SDL_max(1,(int)(stream->src.freq*stream->ratio)),stream->ingest.freq,ehead?ehead->phase:0,0);
        if (n_emit < 0) n_emit = 0;
        if (n_emit > (long long)(0x7FFFFFFF) / ifb)
            return SDL_SetError("audio backlog overflow");
        if (!__au_backlog_reserve(stream, (int)(n_emit * ifb), &allocs)) return 0;
        if (ehead == NULL) {
            node = (__SDL_AuLedger *)__au_calloc(1, sizeof(__SDL_AuLedger));
            if (!node) return SDL_SetError("Out of memory");
            node->src_fb = sfb;
            node->dev_fb = ifb;
            node->src_freq = SDL_max(1,(int)(stream->src.freq*stream->ratio));
            node->dev_freq = stream->ingest.freq;
        }
        if (!__au_extent_reserve(stream, &stream->ingest, (const unsigned char *)buf, len, &det, &allocs)) {
            free(node);
            return 0;
        }
        /* commit — nothing below can fail: the backlog capacity for the whole
           eager emit is reserved, so __au_dev_emit's reserve is a no-op */
        __au_alloc_count += allocs;
        if (node) { __au_led_link(stream, node); __au_alloc_count++; }
        __au_extent_commit(stream, det, (const unsigned char *)buf, len);
        stream->put_orig += len;
        stream->led_tail->orig_frames = stream->head->base + stream->head->frames;
        __au_dev_emit(stream, stream->head, n_emit);
        __sdl_stream_pump(stream);
        return 1;
    }
}
bool SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len){if(!SDL_LockAudioStream(stream))return 0;int before=stream->put_callback&&stream->kind==0?SDL_GetAudioStreamAvailable(stream):0;bool result=__locked_SDL_PutAudioStreamData(stream,buf,len);if(result&&stream->put_callback){int after=stream->kind==0?SDL_GetAudioStreamAvailable(stream):before+len;int added=SDL_max(0,after-before);stream->put_callback(stream->put_userdata,stream,added,added);}SDL_UnlockAudioStream(stream);return result;}


bool SDL_PutAudioStreamDataNoCopy(SDL_AudioStream *s,const void *buf,int len,SDL_AudioStreamDataCompleteCallback callback,void *userdata){
 if(!SDL_LockAudioStream(s))return 0;
 if(!buf||len<0||!s->has_src||!s->has_dst||len%__au_frame_bytes(&s->src)){SDL_UnlockAudioStream(s);return SDL_SetError("Invalid external audio buffer or incomplete format");}
 if(!len){if(callback)callback(userdata,buf,len);SDL_UnlockAudioStream(s);return 1;}
 if(s->kind==1&&(int)(s->src.freq*s->ratio)!=s->ingest.freq){SDL_UnlockAudioStream(s);return SDL_SetError("External device audio buffers require matching source and device sample rates");}
 if(s->kind==1&&!__au_dev_finalize_epoch(s)){SDL_UnlockAudioStream(s);return 0;}
 __SDL_AuExtent *e=__au_calloc(1,sizeof(*e));
 if(!e){SDL_UnlockAudioStream(s);return SDL_SetError("Out of memory");}
 int before=s->put_callback&&s->kind==0?SDL_GetAudioStreamAvailable(s):0;
 e->src=s->src;e->dst=s->dst;e->data=(unsigned char *)buf;e->external=buf;e->external_len=len;e->complete=callback;e->complete_userdata=userdata;e->frames=len/__au_frame_bytes(&s->src);e->cap=len;e->has_input_map=s->has_input_map;memcpy(e->input_map,s->input_map,sizeof(e->input_map));
 if(s->kind==1){
  e->dst=s->ingest;int frame=__au_frame_bytes(&s->ingest),allocs=0;
  long long n=__au_emit_from(e->frames,__au_rate(s,e),e->dst.freq,0,0);
  if(n>0x7fffffff/frame||!__au_backlog_reserve(s,(int)n*frame,&allocs)){free(e);SDL_UnlockAudioStream(s);return 0;}
  __SDL_AuLedger *node=__au_calloc(1,sizeof(*node));if(!node){free(e);SDL_UnlockAudioStream(s);return SDL_SetError("Out of memory");}
  node->src_fb=__au_frame_bytes(&s->src);node->dev_fb=frame;node->src_freq=__au_rate(s,e);node->dev_freq=e->dst.freq;node->orig_frames=e->frames;
  __au_led_link(s,node);s->put_orig+=len;s->head=s->tail=e;__au_dev_emit(s,e,n);__sdl_stream_pump(s);
  if(e->phase>=e->frames*e->dst.freq)__au_dev_finalize_epoch(s);
  if(s->put_callback)s->put_callback(s->put_userdata,s,(int)n*frame,(int)n*frame);
  SDL_UnlockAudioStream(s);return 1;
 }
 if(s->tail)s->tail->next=e;else s->head=e;s->tail=e;
 if(s->put_callback){int added=SDL_max(0,SDL_GetAudioStreamAvailable(s)-before);s->put_callback(s->put_userdata,s,added,added);}
 SDL_UnlockAudioStream(s);return 1;
}
bool SDL_PutAudioStreamPlanarData(SDL_AudioStream *s,const void * const *buffers,int channels,int samples){
 if(!SDL_LockAudioStream(s))return 0;
 if(!buffers||channels< -1||samples<0||!s->has_src){SDL_UnlockAudioStream(s);return SDL_SetError("Invalid planar audio input");}
 int count=s->src.channels,width=__au_fmt_bytes(s->src.format),frame=count*width;
 if(samples>0x7fffffff/frame){SDL_UnlockAudioStream(s);return SDL_SetError("Planar audio buffer is too large");}
 if(channels<0)channels=count;
 if(!samples){SDL_UnlockAudioStream(s);return 1;}
 unsigned char *data=__au_malloc(samples*frame);if(!data){SDL_UnlockAudioStream(s);return SDL_SetError("Out of memory");}
 memset(data,s->src.format==SDL_AUDIO_U8?128:0,samples*frame);
 for(int c=0;c<count&&c<channels;c++)if(buffers[c])for(int i=0;i<samples;i++)memcpy(data+i*frame+c*width,(const unsigned char *)buffers[c]+i*width,width);
 bool ok=SDL_PutAudioStreamData(s,data,samples*frame);free(data);SDL_UnlockAudioStream(s);return ok;
}

static int __locked_SDL_GetAudioStreamData(SDL_AudioStream *stream, void *voidbuf, int len) {
    unsigned char *buf = (unsigned char *)voidbuf;
    int written = 0;
    if (!stream) { SDL_InvalidParamError("stream"); return -1; }
    if (!buf) { SDL_InvalidParamError("buf"); return -1; }
    if (len < 0) { SDL_InvalidParamError("len"); return -1; }
    if (len == 0) return 0;
    if (stream->kind == 1) {
        SDL_SetError("SDL_GetAudioStreamData: not supported on a device stream (the device consumes the converted side)");
        return -1;
    }
    if (!stream->has_src) { SDL_SetError("Stream has no source format"); return -1; }
    if (!stream->has_dst) { SDL_SetError("Stream has no destination format"); return -1; }
    if(stream->get_callback&&!stream->callback_active){int dfb=__au_frame_bytes(&stream->dst),sfb=__au_frame_bytes(&stream->src);long long frames=len/dfb,available=SDL_GetAudioStreamAvailable(stream)/dfb;long long sf=SDL_max(1,(int)(stream->src.freq*stream->ratio));long long total=(frames*sf+stream->dst.freq-1)/stream->dst.freq;long long extra=(SDL_max(0,frames-available)*sf+stream->dst.freq-1)/stream->dst.freq;if(sf!=stream->dst.freq&&extra)extra++;total*=sfb;extra*=sfb;stream->callback_active=1;stream->get_callback(stream->get_userdata,stream,(int)SDL_min(extra,0x7fffffffLL),(int)SDL_min(total,0x7fffffffLL));stream->callback_active=0;}
    while (stream->head) {
        __SDL_AuExtent *e = stream->head;
        int final = __au_extent_final(stream, e);
        long long total = e->k+__au_emit_from(e->base+e->frames,__au_rate(stream,e),e->dst.freq,e->phase,final);
        long long remain = total - e->k;
        int dfb = __au_frame_bytes(&e->dst);
        long long fit = (long long)(len - written) / dfb;
        long long n;
        if (remain <= 0) {
            if (final) {   /* fully drained epoch: release it, walk on */
                __au_carry_phase(e);
                stream->head = e->next;
                if (!stream->head) stream->tail = NULL;
                __au_free_extent(e);
                continue;
            }
            break;         /* unflushed tail — more input may still arrive */
        }
        if (fit <= 0) break;   /* only whole frames of the HEAD epoch; a later
                                  epoch is reached only when a whole frame of
                                  it still fits */
        n = remain < fit ? remain : fit;
        __au_convert(stream,e, n, buf + written);
        written += (int)(n * dfb);
        if (final && e->k >= total) {
            __au_carry_phase(e);
            stream->head = e->next;
            if (!stream->head) stream->tail = NULL;
            __au_free_extent(e);
        }else if(e->external&&e->complete&&e->phase>=(e->base+e->frames)*e->dst.freq){SDL_AudioStreamDataCompleteCallback complete=e->complete;e->complete=NULL;complete(e->complete_userdata,e->external,e->external_len);}
    }
    return written;
}
int SDL_GetAudioStreamData(SDL_AudioStream *stream, void *voidbuf, int len){if(!SDL_LockAudioStream(stream))return -1;int result=__locked_SDL_GetAudioStreamData(stream,voidbuf,len);SDL_UnlockAudioStream(stream);return result;}

static int __locked_SDL_GetAudioStreamAvailable(SDL_AudioStream *stream) {
    long long sum = 0;
    const __SDL_AuExtent *e;
    if (!stream) { SDL_InvalidParamError("stream"); return -1; }
    if (stream->kind == 1) {
        SDL_SetError("SDL_GetAudioStreamAvailable: not supported on a device stream (poll SDL_GetAudioStreamQueued)");
        return -1;
    }
    if (!stream->has_src) { SDL_SetError("Stream has no source format"); return -1; }
    if (!stream->has_dst) { SDL_SetError("Stream has no destination format"); return -1; }
    /* A pure size simulation over the epoch list: local scalars only — no
       decode, no allocation, no phase advance, no host call. Exactly what a
       GetData now could return without an implicit flush, clamped. */
    long long carry=0;
    for (e = stream->head; e; e = e->next) {
        long long phase=e->phase+carry;
        long long remain=__au_emit_from(e->base+e->frames,__au_rate(stream,e),e->dst.freq,phase,__au_extent_final(stream,e));
        carry=__au_contiguous(e)?phase+remain*__au_rate(stream,e)-(e->base+e->frames)*e->dst.freq:0;
        int dfb = __au_frame_bytes(&e->dst);
        if (remain <= 0) continue;
        if (remain > (0x7FFFFFFFLL - sum) / dfb) return 0x7FFFFFFF;
        sum += remain * dfb;
    }
    return (int)sum;
}
int SDL_GetAudioStreamAvailable(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return -1;int result=__locked_SDL_GetAudioStreamAvailable(stream);SDL_UnlockAudioStream(stream);return result;}

static int __locked_SDL_GetAudioStreamQueued(SDL_AudioStream *stream) {
    if (!stream) { SDL_InvalidParamError("stream"); return -1; }
    if (stream->kind == 0) {
        /* Exact original bytes not yet fully consumed by conversion. */
        long long sum = 0;
        const __SDL_AuExtent *e;
        for (e = stream->head; e; e = e->next) {
            long long left = (e->base + e->frames - e->retired) * __au_frame_bytes(&e->src);
            sum += left;
            if (sum >= 0x7FFFFFFFLL) return 0x7FFFFFFF;
        }
        return (int)sum;
    }
    /* DEVICE: reconcile the host once, then report exact original bytes not
       yet acknowledged consumed (identity streams: byte-identical to the old
       ring + backlog number). */
    __au_dev_reconcile(stream);
    {
        long long q = stream->put_orig - stream->retired_orig;
        if (q < 0) q = 0;
        if (q > 0x7FFFFFFFLL) q = 0x7FFFFFFFLL;
        return (int)q;
    }
}
int SDL_GetAudioStreamQueued(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return -1;int result=__locked_SDL_GetAudioStreamQueued(stream);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_FlushAudioStream(SDL_AudioStream *stream) {
    if (!stream) return SDL_InvalidParamError("stream");
    if (stream->kind == 0) {
        if (!stream->has_src) return SDL_SetError("Stream has no source format");
        if (!stream->has_dst) return SDL_SetError("Stream has no destination format");
        /* An idempotent end-of-generation marker: the tail epoch's finite
           resampler tail becomes emittable; nothing converts here. NOT a
           permanent EOF — the next put starts a new epoch. */
        if (stream->tail && !stream->tail->flushed) stream->tail->flushed = 1;
        return 1;
    }
    __au_dev_reconcile(stream);
    if (!__au_dev_finalize_epoch(stream)) return 0;
    __sdl_stream_pump(stream);
    return 1;
}
bool SDL_FlushAudioStream(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_FlushAudioStream(stream);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_ClearAudioStream(SDL_AudioStream *stream) {
    if (!stream) return SDL_InvalidParamError("stream");
    if (stream->kind == 0) {
        __au_free_extents(stream);   /* input, carry, and flush markers; specs stay */
        return 1;
    }
    /* DEVICE — one API-visible transaction: reconcile the latest drain,
       transport-clear, verify the host really cleared, only then zero the
       accounting. On a failed verify the reconciled ledger is retained and
       the call reports failure — accounting is never zeroed while device
       bytes remain. */
    __au_dev_reconcile(stream);
    if(stream->shared_ring){if(!__sdl_clear_audio_shared(stream->dev))return SDL_SetError("Cannot clear shared audio queue");}else __sdl_clear_queued_audio(stream->dev);
    if (__au_queued(stream) != 0)
        return SDL_SetError("SDL_ClearAudioStream: the device did not clear its queue");
    __au_free_extents(stream);
    __au_free_ledger(stream);
    stream->backlog_len = 0;
    stream->submitted_dev = 0;
    stream->last_host_queued = 0;
    stream->submitted_at_last_query = 0;
    stream->ack_carry = 0;
    stream->put_orig = 0;
    stream->retired_orig = 0;
    return 1;
}
bool SDL_ClearAudioStream(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_ClearAudioStream(stream);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream) {
    if (!stream) return SDL_InvalidParamError("stream");
    if (stream->kind != 1) return SDL_SetError("Stream isn't bound to an audio device");
    stream->paused=0;
    if(stream->shared_ring)__builtin(atomic_store32,stream->shared_ring+2,1);else __sdl_pause_audio_device(stream->dev, 0);
    return 1;
}
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_ResumeAudioStreamDevice(stream);SDL_UnlockAudioStream(stream);return result;}

static bool __locked_SDL_PauseAudioStreamDevice(SDL_AudioStream *stream) {
    if (!stream) return SDL_InvalidParamError("stream");
    if (stream->kind != 1) return SDL_SetError("Stream isn't bound to an audio device");
    stream->paused=1;
    if(stream->shared_ring)__builtin(atomic_store32,stream->shared_ring+2,0);else __sdl_pause_audio_device(stream->dev, 1);
    return 1;
}
bool SDL_PauseAudioStreamDevice(SDL_AudioStream *stream){if(!SDL_LockAudioStream(stream))return 0;bool result=__locked_SDL_PauseAudioStreamDevice(stream);SDL_UnlockAudioStream(stream);return result;}

void SDL_DestroyAudioStream(SDL_AudioStream *stream) {
    if (!stream) return;
    if(stream->worker_started){SDL_LockAudioStream(stream);stream->worker_quit=1;SDL_UnlockAudioStream(stream);pthread_join(stream->worker,NULL);}
    if (stream->kind == 1) {
        /* One last honest accounting read, then CANCEL: unsubmitted input,
           converter carry, converted backlog, receipts — all discarded
           without another pump. Only SAB-accepted bytes reach AUDIO_CLOSE
           and the kernel's dying-source drain. Callback workers were joined
           above before their ring and callback state can be freed. */
        __au_dev_reconcile(stream);
        if(stream->shared_ring)__sdl_close_audio_device_shared(stream->dev);else __sdl_close_audio_device(stream->dev);
    }
    SDL_LockAudioStream(stream);
    __au_free_extents(stream);
    __au_free_ledger(stream);
    SDL_UnlockAudioStream(stream);
    free(stream->shared_ring);
    free(stream->backlog);
    SDL_DestroyProperties(stream->properties);
    pthread_mutex_destroy(&stream->lock);
    free(stream);
}

SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid,
                                           const SDL_AudioSpec *spec,
                                           SDL_AudioStreamCallback callback,
                                           void *userdata) {
    SDL_AudioSpec defaults={SDL_AUDIO_F32,2,48000};
    if(devid==SDL_AUDIO_DEVICE_DEFAULT_RECORDING){SDL_SetError("Audio recording is unavailable");return NULL;}
    if(!spec)spec=&defaults;
    if(!__au_validate_side(spec,1))return NULL;
    {
        int dev = __sdl_open_audio_device(spec->freq, spec->format, spec->channels);
        int df, dc, dq;
        SDL_AudioStream *s;
        if (dev <= 0) { SDL_SetError("SDL_OpenAudioDeviceStream: host failed to open an audio device"); return NULL; }
        /* Read-only destination query (#529-A): the kernel sink reports
           F32/stereo/48000, a standalone page its Web Audio graph format, a
           sink-less host the requested spec (SDL's dummy-driver contract).
           A host that cannot answer fails the open — a device with an
           uninventable format is not a device. */
        df = __sdl_audio_dst_query(dev, 0);
        dc = __sdl_audio_dst_query(dev, 1);
        dq = __sdl_audio_dst_query(dev, 2);
        if (__au_fmt_bytes(df) == 0 || dc <= 0 || dq <= 0) {
            __sdl_close_audio_device(dev);
            SDL_SetError("SDL_OpenAudioDeviceStream: host cannot report the device's destination format");
            return NULL;
        }
        s = (SDL_AudioStream *)__au_calloc(1, sizeof(SDL_AudioStream));
        if (!s) { __sdl_close_audio_device(dev); SDL_SetError("Out of memory"); return NULL; }
        __au_init_stream(s);
        s->kind = 1;
        s->dev = s->public_dev = dev;
        s->ingest = *spec;
        s->src = *spec;
        s->has_src = 1;
        s->dst.format = df;
        s->dst.channels = dc;
        s->dst.freq = dq;
        s->has_dst = 1;
        if(callback&&!SDL_SetAudioStreamGetCallback(s,callback,userdata)){SDL_DestroyAudioStream(s);return NULL;}
        return s;
    }
}


/* Each device callback runs on a real pthread. The stream's recursive lock
   permits callbacks to supply data without reentering a suspended C allocator. */
static void *__au_worker(void *opaque){
 SDL_AudioStream *s=opaque;
 for(;;){
  SDL_LockAudioStream(s);
  if(s->worker_quit){SDL_UnlockAudioStream(s);break;}
  if(!s->paused){
   __sdl_stream_pump(s);
   int frame=__au_frame_bytes(&s->ingest),target=s->ingest.freq/25;
   int queued=(__au_queued(s)+s->backlog_len)/frame;
   if(s->get_callback&&queued<target){
    int srcframe=__au_frame_bytes(&s->src);
    int total=(int)((double)target*s->src.freq*s->ratio/s->ingest.freq)*srcframe;
    int additional=(int)((double)(target-queued)*s->src.freq*s->ratio/s->ingest.freq)*srcframe;
    s->get_callback(s->get_userdata,s,additional,total);
    __sdl_stream_pump(s);
   }
  }
  SDL_UnlockAudioStream(s);
  struct timespec wait={0,4000000};nanosleep(&wait,NULL);
 }
 return NULL;
}
static int __au_upgrade(SDL_AudioStream *s){
 if(s->shared_ring)return 1;
 if(__au_queued(s)||s->backlog_len||s->head)return SDL_SetError("Cannot enable device callbacks while audio is queued");
 int frame=__au_frame_bytes(&s->ingest);int cap=(262144/frame)*frame,sink[3];int *ring=calloc(1,cap+16);
 if(!ring)return SDL_SetError("Out of memory");
 int dev=__sdl_open_audio_device_shared(s->ingest.freq,s->ingest.format,s->ingest.channels,ring,cap,sink);
 if(dev<=0){free(ring);return SDL_SetError("Shared audio device transport unavailable");}
 int olddev=s->dev;s->dev=dev;s->shared_ring=ring;s->shared_capacity=cap;
 if(pthread_create(&s->worker,NULL,__au_worker,s)!=0){s->dev=olddev;s->shared_ring=NULL;__sdl_close_audio_device_shared(dev);free(ring);return SDL_SetError("Cannot start audio callback thread");}
 s->worker_started=1;__sdl_close_audio_device(olddev);
 __builtin(atomic_store32,ring+2,!s->paused);
 return 1;
}

/* Test-only introspection — deliberately NOT declared in <SDL.h>; oracle
   tests declare the prototype themselves. Pure reads (no host call, no
   allocation): the Available oracle fingerprints stream state before/after.
   Words: [0] kind, [1] has_src, [2] has_dst, [3] extent count, [4]
   head.frames, [5] head.base, [6] head.k, [7] head.retired, [8]
   head.flushed, [9] extent/ledger alloc counter, [10] backlog_len, [11]
   ledger node count, [12] put_orig, [13] retired_orig (64-bit values
   truncate to 32). Returns the number of words filled. */
int __sdl_audiostream_fp(SDL_AudioStream *s, unsigned int *out, int cap) {
    unsigned int w[14];
    int n = 14, i, ec = 0, lc = 0;
    const __SDL_AuExtent *e;
    const __SDL_AuLedger *L;
    if (!s || !out) return 0;
    for (e = s->head; e; e = e->next) ec++;
    for (L = s->led_head; L; L = L->next) lc++;
    w[0] = (unsigned int)s->kind;
    w[1] = (unsigned int)s->has_src;
    w[2] = (unsigned int)s->has_dst;
    w[3] = (unsigned int)ec;
    w[4] = s->head ? (unsigned int)s->head->frames : 0u;
    w[5] = s->head ? (unsigned int)s->head->base : 0u;
    w[6] = s->head ? (unsigned int)s->head->k : 0u;
    w[7] = s->head ? (unsigned int)s->head->retired : 0u;
    w[8] = s->head ? (unsigned int)s->head->flushed : 0u;
    w[9] = (unsigned int)__au_alloc_count;
    w[10] = (unsigned int)s->backlog_len;
    w[11] = (unsigned int)lc;
    w[12] = (unsigned int)s->put_orig;
    w[13] = (unsigned int)s->retired_orig;
    if (n > cap) n = cap;
    for (i = 0; i < n; i++) out[i] = w[i];
    return n;
}

/* -- SDL_MixAudio (pinned release-3.4.0 src/audio/SDL_mixer.c) -------------
   Volume and overlap behavior mirror the pinned source exactly: volume
   quantizes to round-half-away(fvolume * 128) — 0 is a successful no-op and
   the value is deliberately NOT clamped (negative inverts, > 1 amplifies,
   with upstream's exact truncating integer scaling and cast wrap); the F32
   path scales by the raw float volume. Samples process forward one at a
   time, so overlapping regions (dst == src included) get upstream's exact
   forward-iteration result. Integer destinations saturate to their exact
   range; U8 mixes through the arithmetic form of upstream's mix8 bias table
   (clamp(dst + adjusted - 128, 0, 255) — identical values, no 511-byte
   table). Two validations the proposal adds over upstream (which silently
   truncates): a NULL pointer and a length not divisible by the sample width
   are refused with an error. */
bool SDL_MixAudio(Uint8 *dst, const Uint8 *src, SDL_AudioFormat format,
                  Uint32 len, float volume) {
    int samp = __au_fmt_bytes(format);
    int vol;
    if (!dst) return SDL_InvalidParamError("dst");
    if (!src) return SDL_InvalidParamError("src");
    if (!samp) return SDL_SetError("SDL_MixAudio(): unknown audio format");
    if (len % (Uint32)samp != 0)
        return SDL_SetError("SDL_MixAudio(): length is not a multiple of the sample size");
    {   /* (int)SDL_roundf(volume * 128): round half away from zero, in f32 */
        float x = volume * 128.0f;
        vol = (int)(x >= 0.0f ? x + 0.5f : x - 0.5f);
    }
    if (vol == 0) return 1;
    switch (format) {
    case SDL_AUDIO_U8: {
        Uint32 n = len;
        while (n--) {
            int s = ((((int)*src - 128) * vol) / 128) + 128;
            int m = (int)*dst + (int)(Uint8)s - 128;
            if (m < 0) m = 0; else if (m > 255) m = 255;
            *dst = (Uint8)m;
            ++dst; ++src;
        }
        break;
    }
    case SDL_AUDIO_S8: {
        const signed char *s8 = (const signed char *)src;
        signed char *d8 = (signed char *)dst;
        Uint32 n = len;
        while (n--) {
            int sv = (signed char)(((int)*s8 * vol) / 128);
            int m = (int)*d8 + sv;
            if (m > 127) m = 127; else if (m < -128) m = -128;
            *d8 = (signed char)m;
            ++d8; ++s8;
        }
        break;
    }
    case SDL_AUDIO_S16: {
        Uint32 n = len / 2;
        while (n--) {
            short a, b;
            int m;
            memcpy(&a, src, 2);
            memcpy(&b, dst, 2);
            a = (short)(((int)a * vol) / 128);
            m = (int)a + (int)b;
            if (m > 32767) m = 32767; else if (m < -32768) m = -32768;
            b = (short)m;
            memcpy(dst, &b, 2);
            src += 2; dst += 2;
        }
        break;
    }
    case SDL_AUDIO_S32: {
        Uint32 n = len / 4;
        while (n--) {
            int a, b;
            long long m;
            memcpy(&a, src, 4);
            memcpy(&b, dst, 4);
            m = ((long long)a * vol) / 128 + (long long)b;
            if (m > 2147483647LL) m = 2147483647LL;
            else if (m < -2147483648LL) m = -2147483648LL;
            a = (int)m;
            memcpy(dst, &a, 4);
            src += 4; dst += 4;
        }
        break;
    }
    default: {   /* SDL_AUDIO_F32 */
        Uint32 n = len / 4;
        while (n--) {
            float a, b, m;
            memcpy(&a, src, 4);
            memcpy(&b, dst, 4);
            m = a * volume + b;
            if (m > 1.0f) m = 1.0f; else if (m < -1.0f) m = -1.0f;
            memcpy(dst, &m, 4);
            src += 4; dst += 4;
        }
        break;
    }
    }
    return 1;
}

void SDL_DestroyWindow(SDL_Window *window) {
    if (!window) return;
    /* A window not in the registry is already destroyed (#497): refuse the
       double-destroy rather than double-free. */
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return; }
    /* SDL3 contract (#794): windows parented to this one are destroyed
       first, recursively — the kernel cascades the surfaces regardless, so
       the veneer must retire their records too or the app is left holding
       live-looking pointers to dead surfaces. Popups (anchored children)
       ride the same rule; they always did on the kernel side. */
    for (int i = 0; i < __SDL_MAX_WINDOWS; i++) {
        SDL_Window *c = __sdl_window_registry[i];
        if (c && c != window && c->parent_handle == window->handle) SDL_DestroyWindow(c);
    }
    __sdl_window_unregister(window);
    __sdl_destroy_window(window->handle);
    __sdl_surface_cleanup(&window->surface);
    free(window->surface.pixels);
    free(window->present_pixels);
    free(window);
}

void SDL_Quit(void) {
    __sdl_cancel_timers();
    __sdl_initted = 0;
    __sdl_host_inited = 0;   /* next SDL_Init re-baselines the tick clock */
    __sdl_quit();
}

void SDL_Delay(Uint32 ms) {
    __sdl_delay((int)ms);
}

Uint64 SDL_GetTicks(void) {
    /* f64 ms since SDL_Init → Uint64. Value is a non-negative integer well below
       2^53, so the double→long long→Uint64 path is exact (no 32-bit wrap). */
    return (Uint64)(long long)__sdl_get_ticks();
}

/* ---- Sub-millisecond timing (#495) ----
   Same clock as SDL_GetTicks, without the truncation to whole ms: the host
   returns fractional ms (performance.now()-backed), __sdl_now_ns converts to
   ns. The performance counter IS that ns clock — SDL3 does the same on POSIX,
   where the counter is clock_gettime ns and the frequency is 1e9. */

Uint64 SDL_GetTicksNS(void) {
    return __sdl_now_ns();
}

Uint64 SDL_GetPerformanceCounter(void) {
    return __sdl_now_ns();
}

Uint64 SDL_GetPerformanceFrequency(void) {
    return 1000000000ULL;
}

/* ---- Input state snapshots (#493) ---- */

const bool *SDL_GetKeyboardState(int *numkeys) {
    if (numkeys) *numkeys = SDL_SCANCODE_COUNT;
    return __sdl_key_state;
}

SDL_Keymod SDL_GetModState(void) {
    return __sdl_mod_state;
}

SDL_MouseButtonFlags SDL_GetMouseState(float *x, float *y) {
    if (x) *x = __sdl_mx;
    if (y) *y = __sdl_my;
    return __sdl_mouse_button_state;
}

SDL_Window *SDL_GetMouseFocus(void){return __sdl_window_by_handle(__sdl_mouse_focus_handle);}
void SDL_SetModState(SDL_Keymod mod){__sdl_mod_state=mod;}
SDL_MouseButtonFlags SDL_GetRelativeMouseState(float *x,float *y){if(x)*x=__sdl_rel_x;if(y)*y=__sdl_rel_y;__sdl_rel_x=__sdl_rel_y=0;return __sdl_mouse_button_state;}

SDL_MouseButtonFlags SDL_GetGlobalMouseState(float *x, float *y) {
    /* Desktop-global cursor position is not knowable from a gucOS process (it
       only sees pointer events routed to its own windows, in client coords),
       so this fails loud per the header note instead of lying with
       window-relative values. */
    if (x) *x = 0;
    if (y) *y = 0;
    SDL_SetError(
        "SDL_GetGlobalMouseState: global mouse position is not available in this "
        "runtime (a process only sees pointer events routed to its own windows); "
        "use SDL_GetMouseState for window-relative position");
    return 0;
}

/* ---- Clipboard (docs/archive/0090) ----
   Thin wrappers over the host's __clip_* primitives (kernel slot under the
   OS, process-local slot standalone — host.js createClipboard; the imports
   are declared with the other __sdl_* imports above). fmt 1 is UTF-8 text;
   the kernel slot carries a format tag so richer formats can ride it
   later. Deliberately independent of SDL_Init. */
#define __CLIP_TEXT 1

bool SDL_SetClipboardText(const char *text) {
    int n = text ? (int)strlen(text) : 0;
    if (__clip_set(n ? __CLIP_TEXT : 0, text, n) != 0)
        return SDL_SetError("SDL_SetClipboardText: host clipboard failed");
    return 1;
}

char *SDL_GetClipboardText(void) {
    /* __clip_get returns the TOTAL length (filling at most cap): size with
       cap 0, then read, retrying if another process grew the slot between
       the two calls. Never returns NULL (SDL3 contract): "" when empty. */
    int total = __clip_get(__CLIP_TEXT, 0, 0);
    if (total < 0) total = 0;
    for (;;) {
        char *buf = (char *)malloc((size_t)total + 1);
        if (!buf) { SDL_SetError("Out of memory"); return (char *)calloc(1, 1); }
        int now = total > 0 ? __clip_get(__CLIP_TEXT, buf, total) : 0;
        if (now < 0) now = 0;
        if (now <= total) {
            buf[now < total ? now : total] = 0;
            return buf;
        }
        free(buf);
        total = now;
    }
}

bool SDL_HasClipboardText(void) {
    /* Peek, not a read: term's Edit menu (and every WM_INITMENUPOPUP
       graying via IsClipboardFormatAvailable) calls this per menu open —
       it must serve the cached slot, never park on a host refresh. */
    return __clip_has(__CLIP_TEXT) > 0;
}

bool SDL_ClearClipboardData(void) {
    return __clip_set(0, 0, 0) == 0;
}

void SDL_free(void *mem) { free(mem); }

/* ---- Allocation family (#601) ----
   SDL_free landed with the clipboard (docs/archive/0090); these complete the family
   over the same libc heap, with SDL3's zero-size contract (a zero request is
   bumped to 1, so the return is never NULL for lack of size). */

void *SDL_malloc(size_t size) {
    return malloc(size ? size : 1);
}

void *SDL_calloc(size_t nmemb, size_t size) {
    if (!nmemb || !size) { nmemb = 1; size = 1; }
    return calloc(nmemb, size);
}

void *SDL_realloc(void *mem, size_t size) {
    return realloc(mem, size ? size : 1);
}

/* ---- Pseudo-random numbers (#601) ----
   Upstream SDL3's exact generator (SDL_stdinc: the LCG upstream selected via
   PractRand/TestU01 — state*0xff1cd035+0x05, high 32 bits out), NOT a libc
   rand alias: SDL_rand(n)'s [0,n) contract is a 32x32 high multiply (no
   modulo), and its seeding is independent of libc srand. SDL_srand(0) seeds
   from the performance counter, as upstream. */
static Uint64 __sdl_rand_state;
static bool __sdl_rand_seeded = 0;

void SDL_srand(Uint64 seed) {
    if (!seed) seed = SDL_GetPerformanceCounter();
    __sdl_rand_state = seed;
    __sdl_rand_seeded = 1;
}

Uint32 SDL_rand_bits(void) {
    if (!__sdl_rand_seeded) SDL_srand(0);
    __sdl_rand_state = __sdl_rand_state * 0xff1cd035ULL + 0x05;
    return (Uint32)(__sdl_rand_state >> 32);
}

Sint32 SDL_rand(Sint32 n) {
    return (Sint32)(((Uint64)SDL_rand_bits() * (Uint64)n) >> 32);
}

float SDL_randf(void) {
    /* The top 23 bits scaled by 2^-23 — uniform in [0, 1), as upstream. */
    return (float)(SDL_rand_bits() >> 9) * (1.0f / 8388608.0f);
}

/* SDL_Log (#601): SDL3's INFO-priority app log — formatted message to stderr
   with a trailing newline (upstream's default output function; INFO carries
   no prefix). */
void SDL_Log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ---- Base / pref paths (#601) ----
   SDL_GetBasePath resolves the running binary's directory the way the exec
   actually found it (the kernel32 proc_info_init / user32 res_chase
   precedent): argv[0] from the synthetic /proc/<pid>/cmdline, a bare name
   re-run through $PATH, then realpath to chase symlinks — a gucman
   /usr/local/bin link must yield the package's REAL bin dir. SDL3 ownership:
   the returned pointer is cached and owned by SDL (do not free). Fails loud
   (NULL + error) where there is no /proc (no-kernel flavors). */
static char *__sdl_base_path;

const char *SDL_GetBasePath(void) {
    if (__sdl_base_path) return __sdl_base_path;
    char proc[64], argv0[1024];
    snprintf(proc, sizeof proc, "/proc/%d/cmdline", getpid());
    int fd = open(proc, O_RDONLY);
    if (fd < 0) {
        SDL_SetError("SDL_GetBasePath: no /proc/<pid>/cmdline in this runtime");
        return NULL;
    }
    long n = read(fd, argv0, sizeof argv0 - 1);
    close(fd);
    if (n <= 0) {
        SDL_SetError("SDL_GetBasePath: empty /proc/<pid>/cmdline");
        return NULL;
    }
    argv0[n] = 0;               /* first NUL-terminated entry = argv[0] */
    char abs[1024];
    if (argv0[0] == '/') {
        snprintf(abs, sizeof abs, "%s", argv0);
    } else if (!strchr(argv0, '/')) {
        /* Bare name: the exec found it on $PATH — re-run that search
           (cwd-joining it would invent a file that never existed, the
           kernel32 0048 lesson). */
        const char *path = getenv("PATH");
        if (!path || !*path) path = "/usr/local/bin:/bin";
        abs[0] = 0;
        while (*path) {
            const char *colon = strchr(path, ':');
            int dlen = colon ? (int)(colon - path) : (int)strlen(path);
            char cand[1024];
            snprintf(cand, sizeof cand, "%.*s/%s", dlen, path, argv0);
            struct stat st;
            if (stat(cand, &st) == 0 && S_ISREG(st.st_mode)) {
                snprintf(abs, sizeof abs, "%s", cand);
                break;
            }
            path = colon ? colon + 1 : path + dlen;
        }
        if (!abs[0]) {
            SDL_SetError("SDL_GetBasePath: cannot resolve argv[0] '%s' on PATH", argv0);
            return NULL;
        }
    } else {
        char cwd[512];
        if (!getcwd(cwd, sizeof cwd)) strcpy(cwd, "/");
        snprintf(abs, sizeof abs, "%s%s%s", cwd, strcmp(cwd, "/") == 0 ? "" : "/", argv0);
    }
    char real[1024];
    if (!realpath(abs, real)) snprintf(real, sizeof real, "%s", abs);
    char *slash = strrchr(real, '/');
    if (!slash) {
        SDL_SetError("SDL_GetBasePath: unresolvable path '%s'", real);
        return NULL;
    }
    slash[1] = 0;               /* keep the trailing slash */
    __sdl_base_path = strdup(real);
    if (!__sdl_base_path) { SDL_SetError("Out of memory"); return NULL; }
    return __sdl_base_path;
}

/* A writable per-app dir under $HOME (the XDG data-home shape upstream uses
   on POSIX: $HOME/.local/share/<org>/<app>/), created mkdir -p style,
   returned WITH the trailing slash. Caller frees with SDL_free. */
char *SDL_GetPrefPath(const char *org, const char *app) {
    if (!app || !*app) { SDL_InvalidParamError("app"); return NULL; }
    const char *home = getenv("HOME");
    if (!home || !*home) { SDL_SetError("SDL_GetPrefPath: HOME is not set"); return NULL; }
    char full[1024];
    if (org && *org)
        snprintf(full, sizeof full, "%s/.local/share/%s/%s/", home, org, app);
    else
        snprintf(full, sizeof full, "%s/.local/share/%s/", home, app);
    /* mkdir -p: create every component after $HOME (an existing component is
       fine — the result is verified as a directory at the end, instead of
       tracking per-call errno). */
    for (char *p = full + strlen(home) + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        mkdir(full, 0700);
        *p = '/';
    }
    struct stat st;
    if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) {
        SDL_SetError("SDL_GetPrefPath: could not create '%s'", full);
        return NULL;
    }
    char *out = strdup(full);
    if (!out) SDL_SetError("Out of memory");
    return out;
}

bool SDL_SetWindowTitle(SDL_Window *window, const char *title) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    if(!title)return SDL_InvalidParamError("title");
    if(!SDL_SetStringProperty(SDL_GetWindowProperties(window),"gucos.window.title",title))return 0;
    __sdl_set_window_title(window->handle, title);
    return 1;
}

/* ---- Relative mouse mode (docs/archive/0018) ----
   The REQUESTED mode is tracked here (SDL3: Get returns what Set asked for);
   the host arms the actual pointer-lock machinery (browser: lock on the next
   click into the window; the user can drop the lock with ESC and re-lock by
   clicking back in — the requested mode is unchanged throughout). While the
   pointer is locked the host pushes motion with true relative deltas
   (__sdl_push_mouse_motion_rel_event) instead of absolute positions. */
bool SDL_SetWindowRelativeMouseMode(SDL_Window *window, bool enabled) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    window->relative_mouse = enabled ? 1 : 0;
    __sdl_set_relative_mouse_mode(window->handle, window->relative_mouse);
    return 1;
}

bool SDL_GetWindowRelativeMouseMode(SDL_Window *window) {
    if (!__sdl_window_live(window)) return SDL_InvalidParamError("window");
    return window->relative_mouse;
}

/* ---- Cursors (docs/archive/0105) ----
   A cursor object is just its shape id. The active cursor + visibility are
   application-global (SDL semantics); the effective shape pushed to the host
   is the current cursor's shape while visible, -1 (hidden) otherwise. With
   one window per process, the host applies it to that process's surface.
   __sdl_current_window() finds it (newest window; 0 = no window yet). */
struct SDL_Cursor { int shape; };

static SDL_Cursor __sdl_default_cursor = { SDL_SYSTEM_CURSOR_DEFAULT };
static SDL_Cursor *__sdl_active_cursor = &__sdl_default_cursor;
static int __sdl_cursor_shown = 1;

static int __sdl_cursor_win_handle(void) {
    for (int i = __SDL_MAX_WINDOWS - 1; i >= 0; i--)
        if (__sdl_window_registry[i]) return __sdl_window_registry[i]->handle;
    return 0;
}

static void __sdl_cursor_apply(void) {
    int shape = __sdl_cursor_shown && __sdl_active_cursor
        ? __sdl_active_cursor->shape : -1;
    __sdl_set_cursor(__sdl_cursor_win_handle(), shape);
}

SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id) {
    if (id < 0 || id >= SDL_SYSTEM_CURSOR_COUNT) {
        SDL_SetError("SDL_CreateSystemCursor: bad id");
        return NULL;
    }
    SDL_Cursor *c = (SDL_Cursor *)malloc(sizeof(SDL_Cursor));
    if (!c) return NULL;
    c->shape = (int)id;
    return c;
}

bool SDL_SetCursor(SDL_Cursor *cursor) {
    /* SDL3: NULL just forces a redraw of the current cursor. */
    if (cursor) __sdl_active_cursor = cursor;
    __sdl_cursor_apply();
    return 1;
}

SDL_Cursor *SDL_GetCursor(void) { return __sdl_active_cursor; }
SDL_Cursor *SDL_GetDefaultCursor(void) { return &__sdl_default_cursor; }

void SDL_DestroyCursor(SDL_Cursor *cursor) {
    if (!cursor || cursor == &__sdl_default_cursor) return;
    if (cursor == __sdl_active_cursor) {
        __sdl_active_cursor = &__sdl_default_cursor;
        __sdl_cursor_apply();
    }
    free(cursor);
}

bool SDL_ShowCursor(void) {
    __sdl_cursor_shown = 1;
    __sdl_cursor_apply();
    return 1;
}

bool SDL_HideCursor(void) {
    __sdl_cursor_shown = 0;
    __sdl_cursor_apply();
    return 1;
}

bool SDL_CursorVisible(void) { return __sdl_cursor_shown ? 1 : 0; }

/* ---- SDL_Renderer (2D accelerated) ----
   Opaque to user code; carries the host renderer handle. SDL_Texture is a
   complete type (SDL.h) so programs can read tex->w/h; __handle is the host id.
   Every draw flattens to __sdl_render_quad (4 dst corners + src rect). */
struct SDL_Renderer {
    int handle;
    SDL_Window *window;  /* backref so SDL_DestroyRenderer can clear the
                            window's one-renderer slot (#497) */
    Uint8 draw_r, draw_g, draw_b, draw_a;  /* last-set draw color (#601
                            SDL_GetRenderDrawColor); the create-time default
                            is white, matching both upstream SDL3 and the
                            host renderer's initial drawColor [1,1,1,1] */
    SDL_Texture *target;  /* current render target, NULL = the window (#496) */
    SDL_Texture *debug_atlas;  /* lazy 8x8 debug-font atlas, one per renderer
                            (#494, upstream SDL_render.c shape); owned here —
                            SDL_DestroyRenderer reclaims it */
    int vsync;            /* SDL_SetRenderVSync mode (#500); 0 = disabled, the
                            create-time default per upstream SDL3. GetRenderVSync
                            round-trips THIS value — what was set, never the
                            platform's own cadence. */
    Uint32 __magic;       /* __SDL_REN_MAGIC while live; cleared by
                            SDL_DestroyRenderer before the free (#711 — the
                            __SDL_TEX_MAGIC pattern, same best-effort caveat) */
};

/* Renderer liveness (#711, closing #497's residue): every entry point taking
   an SDL_Renderer* validates it here, so a draw call on a DESTROYED renderer
   — the ordinary destroy-then-one-more-frame bug of a level transition or
   restart path — fails with "Parameter 'renderer' is invalid" instead of
   reporting success with an empty SDL_GetError(). The window conjunct makes
   a renderer whose WINDOW was destroyed equally dead (upstream destroys the
   renderer with its window; here the dangling backref would otherwise keep
   "succeeding" against a freed surface). Magic check is best-effort against
   freed memory, exactly like __sdl_texture_live. */
#define __SDL_REN_MAGIC 0x5352454eu  /* 'SREN' */
static bool __sdl_renderer_live(SDL_Renderer *r) {
    return r && r->__magic == __SDL_REN_MAGIC && __sdl_window_live(r->window);
}

/* ---- Hints (#551): a tiny grow-only store. SDL_GetHint prefers the
   process ENVIRONMENT — upstream gives an env var of the hint's own name
   override priority over a normal SDL_SetHint, and that rule is exactly
   what makes 'SDL_RENDER_DRIVER=software ./game' work on unmodified
   third-party source. ---- */
struct __sdl_hint { char *name; char *value; struct __sdl_hint *next; };
static struct __sdl_hint *__sdl_hints;

bool SDL_SetHint(const char *name, const char *value) {
    if (!name || !*name) { SDL_InvalidParamError("name"); return 0; }
    struct __sdl_hint *h = __sdl_hints;
    while (h && strcmp(h->name, name) != 0) h = h->next;
    if (!h) {
        h = (struct __sdl_hint *)malloc(sizeof(struct __sdl_hint));
        if (!h) { SDL_SetError("Out of memory"); return 0; }
        h->name = strdup(name);
        h->value = NULL;
        h->next = __sdl_hints;
        __sdl_hints = h;
    }
    free(h->value);
    h->value = value ? strdup(value) : NULL;
    return 1;
}

const char *SDL_GetHint(const char *name) {
    if (!name || !*name) return NULL;
    const char *e = getenv(name);
    if (e) return e;
    for (struct __sdl_hint *h = __sdl_hints; h; h = h->next)
        if (strcmp(h->name, name) == 0) return h->value;
    return NULL;
}

/* The render drivers this runtime has (#497 name honesty, #551 opt-in).
   "gucos" is the default driver (GPU-accelerated in the browser OS flavor,
   the CPU rasterizer headless). "software" is upstream SDL3's real software
   driver name: the CPU rasterizer drawing into the window's shm surface —
   NEVER auto-selected (jku veto, #551: a silent GPU→CPU swap is forbidden;
   the explicit request is the sanctioned escape for blocking-loop apps,
   and the #551 refusal message teaches it). NULL falls through to
   SDL_HINT_RENDER_DRIVER (the SDL_RENDER_DRIVER env var feeds it, as
   upstream), then to the default. Asking for any OTHER backend
   ("opengl", "metal", …) must still FAIL ("Couldn't find matching render
   driver"), not silently hand back a different backend while reporting
   success — a developer probing for capabilities would get a false
   positive. */
#define __SDL_RENDER_DRIVER "gucos"

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, const char *name) {
    if (!__sdl_window_live(window)) { SDL_InvalidParamError("window"); return NULL; }
    if (window->renderer) { SDL_SetError("Renderer already associated with window"); return NULL; }
    if (!name || !*name) name = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
    int software = 0;
    if (name && *name) {
        if (strcmp(name, "software") == 0) software = 1;
        else if (strcmp(name, __SDL_RENDER_DRIVER) != 0) {
            SDL_SetError("Couldn't find matching render driver");
            return NULL;
        }
    }
    int h = __sdl_create_renderer(window->handle, software);
    if (h <= 0) { SDL_SetError("SDL_CreateRenderer: host failed to create a renderer"); return NULL; }
    SDL_Renderer *r = (SDL_Renderer *)malloc(sizeof(SDL_Renderer));
    if (!r) { SDL_SetError("Out of memory"); return NULL; }
    r->handle = h;
    r->window = window;
    r->draw_r = r->draw_g = r->draw_b = r->draw_a = 255;
    r->target = NULL;
    r->debug_atlas = NULL;
    r->vsync = 0;   /* SDL3: vsync defaults to SDL_RENDERER_VSYNC_DISABLED (#500) */
    r->__magic = __SDL_REN_MAGIC;
    window->renderer = r;
    return r;
}

void SDL_DestroyRenderer(SDL_Renderer *renderer) {
    /* NULL is SDL3's documented no-op; a dead tag is a double-destroy —
       refuse it rather than double-free (#711, the SDL_DestroyTexture
       convention). NB only the magic gates here, not __sdl_renderer_live:
       destroying a live renderer AFTER its window is the ordinary teardown
       order and must still reclaim. */
    if (!renderer || renderer->__magic != __SDL_REN_MAGIC) {
        if (renderer) SDL_InvalidParamError("renderer");
        return;
    }
    renderer->__magic = 0;
    if (renderer->debug_atlas) {
        SDL_DestroyTexture(renderer->debug_atlas);  /* before the window backref
                            clears — DestroyTexture walks it for #496 unbinding */
        renderer->debug_atlas = NULL;
    }
    if (__sdl_window_live(renderer->window) && renderer->window->renderer == renderer)
        renderer->window->renderer = NULL;
    __sdl_destroy_renderer(renderer->handle);
    free(renderer);
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h) {
    if (!__sdl_renderer_live(renderer)) { SDL_InvalidParamError("renderer"); return NULL; }
    /* SDL3 rejects degenerate and absurd dimensions at create (#497): a 0x0 or
       negative texture is the classic failed-config-parse output, and a huge
       one would defer a multi-GB failure to far from the call that was wrong.
       8192 is the safe per-dimension cap across this runtime's backends (the
       browser flavor's WebGPU default maxTextureDimension2D). */
    if (w <= 0 || h <= 0) { SDL_SetError("Texture dimensions can't be 0"); return NULL; }
    if (w > 8192 || h > 8192) { SDL_SetError("Texture dimensions are limited to 8192x8192"); return NULL; }
    /* #496: the access value is meaningful now (TARGET gates SDL_SetRenderTarget),
       so an out-of-enum value must fail here — not ride along silently. */
    if (access != SDL_TEXTUREACCESS_STATIC && access != SDL_TEXTUREACCESS_STREAMING &&
        access != SDL_TEXTUREACCESS_TARGET) { SDL_InvalidParamError("access"); return NULL; }
    int th = __sdl_create_texture(renderer->handle, (int)access, w, h);
    if (th <= 0) { SDL_SetError("SDL_CreateTexture: host failed to create a texture (%dx%d)", w, h); return NULL; }
    SDL_Texture *t = (SDL_Texture *)malloc(sizeof(SDL_Texture));
    if (!t) { SDL_SetError("Out of memory"); return NULL; }
    t->format = format;
    t->w = w;
    t->h = h;
    t->__handle = th;
    t->__magic = __SDL_TEX_MAGIC;
    t->__access = (int)access;
    t->__renderer = renderer;
    /* SDL3 default blend mode is alpha-aware: an alpha-format texture defaults to
       SDL_BLENDMODE_BLEND, a non-alpha one to SDL_BLENDMODE_NONE
       (src/render/SDL_render.c). */
    SDL_SetTextureBlendMode(t, SDL_ISPIXELFORMAT_ALPHA(format) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    return t;
}

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface) {
    if (!__sdl_renderer_live(renderer)) { SDL_InvalidParamError("renderer"); return NULL; }
    if (!surface) { SDL_InvalidParamError("surface"); return NULL; }
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
    if (!t) return 0;
    SDL_Surface *converted = NULL;
    SDL_Surface *pixels = surface;
    if (surface->format != SDL_PIXELFORMAT_RGBA32 || SDL_SurfaceHasColorKey(surface)) {
        converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (!converted) { SDL_DestroyTexture(t); return NULL; }
        pixels = converted;
    }
    __sdl_update_texture(t->__handle, pixels->pixels, pixels->pitch, 0, 0, pixels->w, pixels->h);
    Uint8 r = 255, g = 255, b = 255, a = 255;
    SDL_BlendMode blend = SDL_BLENDMODE_NONE;
    SDL_GetSurfaceColorMod(surface, &r, &g, &b);
    SDL_GetSurfaceAlphaMod(surface, &a);
    SDL_GetSurfaceBlendMode(surface, &blend);
    if (SDL_SurfaceHasColorKey(surface)) blend = SDL_BLENDMODE_BLEND;
    SDL_SetTextureColorMod(t, r, g, b);
    SDL_SetTextureAlphaMod(t, a);
    SDL_SetTextureBlendMode(t, blend);
    SDL_DestroySurface(converted);
    return t;
}

void SDL_DestroyTexture(SDL_Texture *texture) {
    if (!__sdl_texture_live(texture)) {
        /* NULL is SDL3's documented no-op; a dead tag is a double-destroy —
           refuse it rather than double-free. */
        if (texture) SDL_InvalidParamError("texture");
        return;
    }
    /* #496: destroying the bound render target implicitly restores the window
       (upstream SDL_render.c). Renderer liveness is best-effort through the
       window backref chain — the same idiom as the __magic tag above. */
    SDL_Renderer *rr = texture->__renderer;
    if (rr && __sdl_window_live(rr->window) && rr->window->renderer == rr && rr->target == texture)
        SDL_SetRenderTarget(rr, NULL);
    texture->__magic = 0;
    __sdl_destroy_texture(texture->__handle);
    free(texture);
}

bool SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (!pixels) return SDL_InvalidParamError("pixels");
    /* SDL3: rect==NULL updates the entire texture; otherwise only that sub-region
       (the source buffer is rect->h rows of 'pitch' bytes). Honour the rect so a
       sub-rect update writes the right pixels AND the host reads only the bytes the
       caller actually provided (the old full-size read was wrong + read OOB). */
    int x = 0, y = 0, w = texture->w, h = texture->h;
    if (rect) {
        x = rect->x; y = rect->y; w = rect->w; h = rect->h;
        if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > texture->w || y + h > texture->h)
            return SDL_SetError("SDL_UpdateTexture: rect is outside the texture bounds");
    }
    __sdl_update_texture(texture->__handle, pixels, pitch, x, y, w, h);
    return 1;
}

bool SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    __sdl_set_texture_color_mod(texture->__handle, r / 255.0, g / 255.0, b / 255.0);
    return 1;
}

bool SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    __sdl_set_texture_alpha_mod(texture->__handle, alpha / 255.0);
    return 1;
}

bool SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    __sdl_set_texture_blend_mode(texture->__handle, (int)blendMode);
    return 1;
}

bool SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (blendMode) *blendMode = (SDL_BlendMode)__sdl_get_texture_blend_mode(texture->__handle);
    return 1;
}

bool SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode scaleMode) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    __sdl_set_texture_scale_mode(texture->__handle, (int)scaleMode);
    return 1;
}

bool SDL_GetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode *scaleMode) {
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (scaleMode) *scaleMode = (SDL_ScaleMode)__sdl_get_texture_scale_mode(texture->__handle);
    return 1;
}

bool SDL_SetRenderClipRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_set_render_clip_rect(renderer->handle, rect != NULL,
        rect ? rect->x : 0, rect ? rect->y : 0, rect ? rect->w : 0, rect ? rect->h : 0);
    return 1;
}
bool SDL_GetRenderClipRect(SDL_Renderer *renderer, SDL_Rect *rect) {
    if (rect) memset(rect, 0, sizeof(*rect));
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (rect) {
        rect->x = __sdl_get_render_clip(renderer->handle, 1);
        rect->y = __sdl_get_render_clip(renderer->handle, 2);
        rect->w = __sdl_get_render_clip(renderer->handle, 3);
        rect->h = __sdl_get_render_clip(renderer->handle, 4);
    }
    return 1;
}
bool SDL_RenderClipEnabled(SDL_Renderer *renderer) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    return __sdl_get_render_clip(renderer->handle, 0) != 0;
}

bool SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    renderer->draw_r = r;
    renderer->draw_g = g;
    renderer->draw_b = b;
    renderer->draw_a = a;
    __sdl_set_draw_color(renderer->handle, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    return 1;
}

bool SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (r) *r = renderer->draw_r;
    if (g) *g = renderer->draw_g;
    if (b) *b = renderer->draw_b;
    if (a) *a = renderer->draw_a;
    return 1;
}

bool SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_set_draw_blend_mode(renderer->handle, (int)blendMode);
    return 1;
}

bool SDL_RenderClear(SDL_Renderer *renderer) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_render_clear(renderer->handle);
    return 1;
}

/* Axis-aligned quad helper from a dst rect (x,y,w,h) + src rect. */
static void __sdl_quad_rect(int r, int texH, float dx, float dy, float dw, float dh,
                            float sx, float sy, float sw, float sh) {
    __sdl_render_quad(r, texH, dx, dy, dx + dw, dy, dx + dw, dy + dh, dx, dy + dh,
                      sx, sy, sw, sh);
}

bool SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                       const SDL_FRect *srcrect, const SDL_FRect *dstrect) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (texture == renderer->target)
        return SDL_SetError("texture is the current render target — it cannot be its own source");
    float sx = 0, sy = 0, sw = (float)texture->w, sh = (float)texture->h;
    if (srcrect) { sx = srcrect->x; sy = srcrect->y; sw = srcrect->w; sh = srcrect->h; }
    float dx = 0, dy = 0, dw = (float)texture->w, dh = (float)texture->h;
    if (dstrect) { dx = dstrect->x; dy = dstrect->y; dw = dstrect->w; dh = dstrect->h; }
    __sdl_quad_rect(renderer->handle, texture->__handle, dx, dy, dw, dh, sx, sy, sw, sh);
    return 1;
}

/* Rotated blit (#672): dstrect's corners rotate 'angle' degrees CLOCKWISE
   (the SDL3 convention; y-down screen coords make the standard rotation
   matrix clockwise for positive angles) about 'center', which is relative to
   dstrect's top-left (NULL = dstrect's middle). 'flip' mirrors the TEXTURE,
   not the geometry — SDL3 swaps the source UVs on the rotated quad — and
   since __sdl_render_quad maps the src rect onto its corner slots in fixed
   TL,TR,BR,BL order, a flip is exactly a permutation of which rotated corner
   is emitted in which slot. */
bool SDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture,
                              const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                              double angle, const SDL_FPoint *center, SDL_FlipMode flip) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (texture == renderer->target)
        return SDL_SetError("texture is the current render target — it cannot be its own source");
    float sx = 0, sy = 0, sw = (float)texture->w, sh = (float)texture->h;
    if (srcrect) { sx = srcrect->x; sy = srcrect->y; sw = srcrect->w; sh = srcrect->h; }
    float dx = 0, dy = 0, dw = (float)texture->w, dh = (float)texture->h;
    if (dstrect) { dx = dstrect->x; dy = dstrect->y; dw = dstrect->w; dh = dstrect->h; }
    float cx = dx + (center ? center->x : dw * 0.5f);
    float cy = dy + (center ? center->y : dh * 0.5f);
    double rad = angle * (3.14159265358979323846 / 180.0);
    float c = (float)cos(rad), s = (float)sin(rad);
    float px[4] = { dx, dx + dw, dx + dw, dx };      /* TL,TR,BR,BL */
    float py[4] = { dy, dy, dy + dh, dy + dh };
    float rx[4], ry[4];
    for (int i = 0; i < 4; i++) {
        float ox = px[i] - cx, oy = py[i] - cy;
        rx[i] = cx + ox * c - oy * s;
        ry[i] = cy + ox * s + oy * c;
    }
    /* Slot i samples src corner i; flip re-aims the sample at the mirrored
       geometric corner. H: TL<->TR, BL<->BR.  V: TL<->BL, TR<->BR. */
    static const int perm[4][4] = {
        { 0, 1, 2, 3 },   /* SDL_FLIP_NONE */
        { 1, 0, 3, 2 },   /* SDL_FLIP_HORIZONTAL */
        { 3, 2, 1, 0 },   /* SDL_FLIP_VERTICAL */
        { 2, 3, 0, 1 },   /* HORIZONTAL|VERTICAL */
    };
    const int *m = perm[flip & 3];
    __sdl_render_quad(renderer->handle, texture->__handle,
                      rx[m[0]], ry[m[0]], rx[m[1]], ry[m[1]],
                      rx[m[2]], ry[m[2]], rx[m[3]], ry[m[3]],
                      sx, sy, sw, sh);
    return 1;
}

/* Unguarded draw bodies (#712 finding A): each singular entry point is its
   liveness guard plus one of these; the batch entry points validate the
   renderer ONCE and loop the bodies directly — liveness cannot change
   mid-loop, and __sdl_renderer_live walks the window registry per call, so
   an N-element batch used to pay a redundant 32N-compare scan. Post-guard a
   body cannot fail, which is why the singulars always return 1. */
static void __sdl_draw_fill_rect(SDL_Renderer *renderer, const SDL_FRect *rect) {
    float dx = 0, dy = 0, dw = 0, dh = 0;
    if (rect) { dx = rect->x; dy = rect->y; dw = rect->w; dh = rect->h; }
    __sdl_quad_rect(renderer->handle, 0, dx, dy, dw, dh, 0, 0, 1, 1);
}

static void __sdl_draw_rect(SDL_Renderer *renderer, const SDL_FRect *rect) {
    if (!rect) return;
    float x = rect->x, y = rect->y, w = rect->w, h = rect->h;
    __sdl_quad_rect(renderer->handle, 0, x, y, w, 1, 0, 0, 1, 1);          /* top */
    __sdl_quad_rect(renderer->handle, 0, x, y + h - 1, w, 1, 0, 0, 1, 1);  /* bottom */
    __sdl_quad_rect(renderer->handle, 0, x, y, 1, h, 0, 0, 1, 1);          /* left */
    __sdl_quad_rect(renderer->handle, 0, x + w - 1, y, 1, h, 0, 0, 1, 1);  /* right */
}

static void __sdl_draw_line(SDL_Renderer *renderer, float x1, float y1, float x2, float y2) {
    float ddx = x2 - x1, ddy = y2 - y1;
    float len = (float)sqrt(ddx * ddx + ddy * ddy);
    if (len < 0.0001f) {
        __sdl_quad_rect(renderer->handle, 0, x1, y1, 1, 1, 0, 0, 1, 1);
        return;
    }
    /* a 1px-thick quad along the line (perpendicular half-width 0.5) */
    float nx = -ddy / len * 0.5f, ny = ddx / len * 0.5f;
    __sdl_render_quad(renderer->handle, 0,
                      x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny,
                      0, 0, 1, 1);
}

bool SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_draw_fill_rect(renderer, rect);
    return 1;
}

bool SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *rect) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_draw_rect(renderer, rect);
    return 1;
}

bool SDL_RenderLine(SDL_Renderer *renderer, float x1, float y1, float x2, float y2) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_draw_line(renderer, x1, y1, x2, y2);
    return 1;
}

bool SDL_RenderPoint(SDL_Renderer *renderer, float x, float y) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    __sdl_quad_rect(renderer->handle, 0, x, y, 1, 1, 0, 0, 1, 1);
    return 1;
}

/* ---- Batch draw calls (#601): loops over the singular bodies, with
   upstream's argument contract (a NULL array is an error; count < 1 — or
   < 2 for the polyline — draws nothing and succeeds). ---- */

bool SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!rects) return SDL_InvalidParamError("rects");
    for (int i = 0; i < count; i++)
        __sdl_draw_fill_rect(renderer, &rects[i]);
    return 1;
}

bool SDL_RenderRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!rects) return SDL_InvalidParamError("rects");
    for (int i = 0; i < count; i++)
        __sdl_draw_rect(renderer, &rects[i]);
    return 1;
}

bool SDL_RenderPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!points) return SDL_InvalidParamError("points");
    for (int i = 0; i < count; i++)
        __sdl_quad_rect(renderer->handle, 0, points[i].x, points[i].y, 1, 1, 0, 0, 1, 1);
    return 1;
}

/* count-1 CONNECTED segments (a polyline), per SDL3 — not count/2 pairs. */
bool SDL_RenderLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!points) return SDL_InvalidParamError("points");
    for (int i = 0; i + 1 < count; i++)
        __sdl_draw_line(renderer, points[i].x, points[i].y,
                        points[i + 1].x, points[i + 1].y);
    return 1;
}

bool SDL_RenderGeometry(SDL_Renderer *renderer, SDL_Texture *texture,
                        const SDL_Vertex *vertices, int num_vertices,
                        const int *indices, int num_indices) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (texture && !__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
    if (texture && texture == renderer->target)
        return SDL_SetError("texture is the current render target — it cannot be its own source");
    if (!vertices) return SDL_InvalidParamError("vertices");
    if (num_vertices < 3) return SDL_SetError("SDL_RenderGeometry: num_vertices must be >= 3");
    if (indices && num_indices % 3 != 0) return SDL_SetError("SDL_RenderGeometry: num_indices must be a multiple of 3");
    /* SDL3 validates every index against num_vertices (#497): an out-of-range
       index — the stale-count index buffer bug — must fail here, not "work" by
       reading whatever sits past the vertex array. */
    if (indices) {
        for (int i = 0; i < num_indices; i++)
            if (indices[i] < 0 || indices[i] >= num_vertices)
                return SDL_SetError("Values of 'indices' out of bounds");
    }
    int n = indices ? num_indices : num_vertices;
    if (n <= 0) return 1;
    /* Resolve indices into a flat triangle soup of [x,y,u,v,r,g,b,a] per vertex
       (host stays struct-ignorant — it just reads floats). */
    float *buf = (float *)malloc((size_t)n * 8 * sizeof(float));
    if (!buf) return SDL_SetError("Out of memory");
    for (int i = 0; i < n; i++) {
        const SDL_Vertex *v = &vertices[indices ? indices[i] : i];
        float *o = buf + (size_t)i * 8;
        o[0] = v->position.x;  o[1] = v->position.y;
        o[2] = v->tex_coord.x; o[3] = v->tex_coord.y;
        o[4] = v->color.r;     o[5] = v->color.g;
        o[6] = v->color.b;     o[7] = v->color.a;
    }
    __sdl_render_geometry(renderer->handle, texture ? texture->__handle : 0, buf, n);
    free(buf);
    return 1;
}

/* bool per upstream SDL3 (#496): the documented present-while-target-bound
   failure has to be reportable. */
bool SDL_RenderPresent(SDL_Renderer *renderer) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (renderer->target)
        return SDL_SetError("SDL_RenderPresent: a render target is bound — call SDL_SetRenderTarget(renderer, NULL) first (presenting to a texture target is not supported)");
    __sdl_render_present(renderer->handle);
    return 1;
}

/* ---- Render targets (#496) ---- */

bool SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (texture) {
        if (!__sdl_texture_live(texture)) return SDL_InvalidParamError("texture");
        if (texture->__access != SDL_TEXTUREACCESS_TARGET)
            return SDL_SetError("texture is not a render target");  /* upstream's message */
    }
    if (renderer->target == texture) return 1;
    renderer->target = texture;
    __sdl_set_render_target(renderer->handle, texture ? texture->__handle : 0);
    return 1;
}

SDL_Texture *SDL_GetRenderTarget(SDL_Renderer *renderer) {
    if (!__sdl_renderer_live(renderer)) { SDL_InvalidParamError("renderer"); return NULL; }
    return renderer->target;
}

/* ---- Renderer vsync (#500) ---- */

bool SDL_SetRenderVSync(SDL_Renderer *renderer, int vsync) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (vsync < 0)   /* adaptive (-1) and friends: nothing here can late-tear */
        return SDL_SetError("SDL_SetRenderVSync: adaptive vsync is not supported (mode unchanged)");
    if (!__sdl_set_render_vsync(renderer->handle, vsync))
        return SDL_SetError("SDL_SetRenderVSync: vsync is not supported here — no display clock (headless or standalone; mode unchanged)");
    renderer->vsync = vsync;
    return 1;
}

bool SDL_GetRenderVSync(SDL_Renderer *renderer, int *vsync) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (vsync) *vsync = renderer->vsync;
    return 1;
}


/* ---- SDL_RenderDebugText (#494) ----
   Upstream SDL3's built-in 8x8 debug font (SDL_render.c + the public-domain
   font in SDL_render_debug_font.h — Marcel Sondaar / IBM VGA fonts), mirrored
   mechanism-for-mechanism: a lazy per-renderer glyph atlas, white-on-
   transparent, tinted per call from the current draw color via texture
   color/alpha mod, one SDL_RenderTexture per glyph, x advances 8. Upstream
   builds the atlas through SDL_CreateSurfaceFrom -> CreateTextureFromSurface;
   the surface CONSTRUCTORS don't exist in this veneer (only TTF/IMG-returned
   surfaces do), so the atlas uploads through SDL_CreateTexture(STATIC) +
   SDL_UpdateTexture — the same net texture state (RGBA32 => BLEND at create,
   NEAREST set explicitly, as upstream). The documented character set is ASCII
   (upstream's contract); the codepoint->glyph mapping below is upstream's
   VERBATIM, including its quirk that raw codepoints >= 190 take the
   "invalid" glyph before the Latin-1 adjustment — faithful-to-source over a
   silent local improvement. */

#define __SDL_DEBUG_FONT_NUM_GLYPHS 190
#define __SDL_DEBUG_FONT_GLYPHS_PER_ROW 14

/* 190 glyphs x 8 bytes, bit 0 = leftmost pixel. Codepoints 0x21-0x7E, then
   0xA1-0xFF, then the "invalid character" checkerboard. */
static const Uint8 __sdl_debug_font_data[__SDL_DEBUG_FONT_NUM_GLYPHS * 8] = {
    0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00,  /* 0x21 */
    0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0x22 */
    0x36,0x36,0x7f,0x36,0x7f,0x36,0x36,0x00,  /* 0x23 */
    0x0c,0x3e,0x03,0x1e,0x30,0x1f,0x0c,0x00,  /* 0x24 */
    0x00,0x63,0x33,0x18,0x0c,0x66,0x63,0x00,  /* 0x25 */
    0x1c,0x36,0x1c,0x6e,0x3b,0x33,0x6e,0x00,  /* 0x26 */
    0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00,  /* 0x27 */
    0x18,0x0c,0x06,0x06,0x06,0x0c,0x18,0x00,  /* 0x28 */
    0x06,0x0c,0x18,0x18,0x18,0x0c,0x06,0x00,  /* 0x29 */
    0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00,  /* 0x2a */
    0x00,0x0c,0x0c,0x3f,0x0c,0x0c,0x00,0x00,  /* 0x2b */
    0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x06,  /* 0x2c */
    0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x00,  /* 0x2d */
    0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x00,  /* 0x2e */
    0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00,  /* 0x2f */
    0x3e,0x63,0x73,0x7b,0x6f,0x67,0x3e,0x00,  /* 0x30 */
    0x0c,0x0e,0x0c,0x0c,0x0c,0x0c,0x3f,0x00,  /* 0x31 */
    0x1e,0x33,0x30,0x1c,0x06,0x33,0x3f,0x00,  /* 0x32 */
    0x1e,0x33,0x30,0x1c,0x30,0x33,0x1e,0x00,  /* 0x33 */
    0x38,0x3c,0x36,0x33,0x7f,0x30,0x78,0x00,  /* 0x34 */
    0x3f,0x03,0x1f,0x30,0x30,0x33,0x1e,0x00,  /* 0x35 */
    0x1c,0x06,0x03,0x1f,0x33,0x33,0x1e,0x00,  /* 0x36 */
    0x3f,0x33,0x30,0x18,0x0c,0x0c,0x0c,0x00,  /* 0x37 */
    0x1e,0x33,0x33,0x1e,0x33,0x33,0x1e,0x00,  /* 0x38 */
    0x1e,0x33,0x33,0x3e,0x30,0x18,0x0e,0x00,  /* 0x39 */
    0x00,0x0c,0x0c,0x00,0x00,0x0c,0x0c,0x00,  /* 0x3a */
    0x00,0x0c,0x0c,0x00,0x00,0x0c,0x0c,0x06,  /* 0x3b */
    0x18,0x0c,0x06,0x03,0x06,0x0c,0x18,0x00,  /* 0x3c */
    0x00,0x00,0x3f,0x00,0x00,0x3f,0x00,0x00,  /* 0x3d */
    0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00,  /* 0x3e */
    0x1e,0x33,0x30,0x18,0x0c,0x00,0x0c,0x00,  /* 0x3f */
    0x3e,0x63,0x7b,0x7b,0x7b,0x03,0x1e,0x00,  /* 0x40 */
    0x0c,0x1e,0x33,0x33,0x3f,0x33,0x33,0x00,  /* 0x41 */
    0x3f,0x66,0x66,0x3e,0x66,0x66,0x3f,0x00,  /* 0x42 */
    0x3c,0x66,0x03,0x03,0x03,0x66,0x3c,0x00,  /* 0x43 */
    0x1f,0x36,0x66,0x66,0x66,0x36,0x1f,0x00,  /* 0x44 */
    0x7f,0x46,0x16,0x1e,0x16,0x46,0x7f,0x00,  /* 0x45 */
    0x7f,0x46,0x16,0x1e,0x16,0x06,0x0f,0x00,  /* 0x46 */
    0x3c,0x66,0x03,0x03,0x73,0x66,0x7c,0x00,  /* 0x47 */
    0x33,0x33,0x33,0x3f,0x33,0x33,0x33,0x00,  /* 0x48 */
    0x1e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0x49 */
    0x78,0x30,0x30,0x30,0x33,0x33,0x1e,0x00,  /* 0x4a */
    0x67,0x66,0x36,0x1e,0x36,0x66,0x67,0x00,  /* 0x4b */
    0x0f,0x06,0x06,0x06,0x46,0x66,0x7f,0x00,  /* 0x4c */
    0x63,0x77,0x7f,0x7f,0x6b,0x63,0x63,0x00,  /* 0x4d */
    0x63,0x67,0x6f,0x7b,0x73,0x63,0x63,0x00,  /* 0x4e */
    0x1c,0x36,0x63,0x63,0x63,0x36,0x1c,0x00,  /* 0x4f */
    0x3f,0x66,0x66,0x3e,0x06,0x06,0x0f,0x00,  /* 0x50 */
    0x1e,0x33,0x33,0x33,0x3b,0x1e,0x38,0x00,  /* 0x51 */
    0x3f,0x66,0x66,0x3e,0x36,0x66,0x67,0x00,  /* 0x52 */
    0x1e,0x33,0x07,0x0e,0x38,0x33,0x1e,0x00,  /* 0x53 */
    0x3f,0x2d,0x0c,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0x54 */
    0x33,0x33,0x33,0x33,0x33,0x33,0x3f,0x00,  /* 0x55 */
    0x33,0x33,0x33,0x33,0x33,0x1e,0x0c,0x00,  /* 0x56 */
    0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00,  /* 0x57 */
    0x63,0x63,0x36,0x1c,0x1c,0x36,0x63,0x00,  /* 0x58 */
    0x33,0x33,0x33,0x1e,0x0c,0x0c,0x1e,0x00,  /* 0x59 */
    0x7f,0x63,0x31,0x18,0x4c,0x66,0x7f,0x00,  /* 0x5a */
    0x1e,0x06,0x06,0x06,0x06,0x06,0x1e,0x00,  /* 0x5b */
    0x03,0x06,0x0c,0x18,0x30,0x60,0x40,0x00,  /* 0x5c */
    0x1e,0x18,0x18,0x18,0x18,0x18,0x1e,0x00,  /* 0x5d */
    0x08,0x1c,0x36,0x63,0x00,0x00,0x00,0x00,  /* 0x5e */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,  /* 0x5f */
    0x0c,0x0c,0x18,0x00,0x00,0x00,0x00,0x00,  /* 0x60 */
    0x00,0x00,0x1e,0x30,0x3e,0x33,0x6e,0x00,  /* 0x61 */
    0x07,0x06,0x06,0x3e,0x66,0x66,0x3b,0x00,  /* 0x62 */
    0x00,0x00,0x1e,0x33,0x03,0x33,0x1e,0x00,  /* 0x63 */
    0x38,0x30,0x30,0x3e,0x33,0x33,0x6e,0x00,  /* 0x64 */
    0x00,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00,  /* 0x65 */
    0x1c,0x36,0x06,0x0f,0x06,0x06,0x0f,0x00,  /* 0x66 */
    0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x1f,  /* 0x67 */
    0x07,0x06,0x36,0x6e,0x66,0x66,0x67,0x00,  /* 0x68 */
    0x0c,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0x69 */
    0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1e,  /* 0x6a */
    0x07,0x06,0x66,0x36,0x1e,0x36,0x67,0x00,  /* 0x6b */
    0x0e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0x6c */
    0x00,0x00,0x33,0x7f,0x7f,0x6b,0x63,0x00,  /* 0x6d */
    0x00,0x00,0x1f,0x33,0x33,0x33,0x33,0x00,  /* 0x6e */
    0x00,0x00,0x1e,0x33,0x33,0x33,0x1e,0x00,  /* 0x6f */
    0x00,0x00,0x3b,0x66,0x66,0x3e,0x06,0x0f,  /* 0x70 */
    0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x78,  /* 0x71 */
    0x00,0x00,0x3b,0x6e,0x66,0x06,0x0f,0x00,  /* 0x72 */
    0x00,0x00,0x3e,0x03,0x1e,0x30,0x1f,0x00,  /* 0x73 */
    0x08,0x0c,0x3e,0x0c,0x0c,0x2c,0x18,0x00,  /* 0x74 */
    0x00,0x00,0x33,0x33,0x33,0x33,0x6e,0x00,  /* 0x75 */
    0x00,0x00,0x33,0x33,0x33,0x1e,0x0c,0x00,  /* 0x76 */
    0x00,0x00,0x63,0x6b,0x7f,0x7f,0x36,0x00,  /* 0x77 */
    0x00,0x00,0x63,0x36,0x1c,0x36,0x63,0x00,  /* 0x78 */
    0x00,0x00,0x33,0x33,0x33,0x3e,0x30,0x1f,  /* 0x79 */
    0x00,0x00,0x3f,0x19,0x0c,0x26,0x3f,0x00,  /* 0x7a */
    0x38,0x0c,0x0c,0x07,0x0c,0x0c,0x38,0x00,  /* 0x7b */
    0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,  /* 0x7c */
    0x07,0x0c,0x0c,0x38,0x0c,0x0c,0x07,0x00,  /* 0x7d */
    0x6e,0x3b,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0x7e */
    0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x00,  /* 0xa1 */
    0x18,0x18,0x7e,0x03,0x03,0x7e,0x18,0x18,  /* 0xa2 */
    0x1c,0x36,0x26,0x0f,0x06,0x67,0x3f,0x00,  /* 0xa3 */
    0x00,0x00,0x63,0x3e,0x36,0x3e,0x63,0x00,  /* 0xa4 */
    0x33,0x33,0x1e,0x3f,0x0c,0x3f,0x0c,0x0c,  /* 0xa5 */
    0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,  /* 0xa6 */
    0x7c,0xc6,0x1c,0x36,0x36,0x1c,0x33,0x1e,  /* 0xa7 */
    0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0xa8 */
    0x3c,0x42,0x99,0x85,0x85,0x99,0x42,0x3c,  /* 0xa9 */
    0x3c,0x36,0x36,0x7c,0x00,0x00,0x00,0x00,  /* 0xaa */
    0x00,0xcc,0x66,0x33,0x66,0xcc,0x00,0x00,  /* 0xab */
    0x00,0x00,0x00,0x3f,0x30,0x30,0x00,0x00,  /* 0xac */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0xad */
    0x3c,0x42,0x9d,0xa5,0x9d,0xa5,0x42,0x3c,  /* 0xae */
    0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0xaf */
    0x1c,0x36,0x36,0x1c,0x00,0x00,0x00,0x00,  /* 0xb0 */
    0x18,0x18,0x7e,0x18,0x18,0x00,0x7e,0x00,  /* 0xb1 */
    0x1c,0x30,0x18,0x0c,0x3c,0x00,0x00,0x00,  /* 0xb2 */
    0x1c,0x30,0x18,0x30,0x1c,0x00,0x00,0x00,  /* 0xb3 */
    0x18,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,  /* 0xb4 */
    0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x03,  /* 0xb5 */
    0xfe,0xdb,0xdb,0xde,0xd8,0xd8,0xd8,0x00,  /* 0xb6 */
    0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,  /* 0xb7 */
    0x00,0x00,0x00,0x00,0x00,0x18,0x30,0x1e,  /* 0xb8 */
    0x08,0x0c,0x08,0x1c,0x00,0x00,0x00,0x00,  /* 0xb9 */
    0x1c,0x36,0x36,0x1c,0x00,0x00,0x00,0x00,  /* 0xba */
    0x00,0x33,0x66,0xcc,0x66,0x33,0x00,0x00,  /* 0xbb */
    0xc3,0x63,0x33,0xbd,0xec,0xf6,0xf3,0x03,  /* 0xbc */
    0xc3,0x63,0x33,0x7b,0xcc,0x66,0x33,0xf0,  /* 0xbd */
    0x03,0xc4,0x63,0xb4,0xdb,0xac,0xe6,0x80,  /* 0xbe */
    0x0c,0x00,0x0c,0x06,0x03,0x33,0x1e,0x00,  /* 0xbf */
    0x07,0x00,0x1c,0x36,0x63,0x7f,0x63,0x00,  /* 0xc0 */
    0x70,0x00,0x1c,0x36,0x63,0x7f,0x63,0x00,  /* 0xc1 */
    0x1c,0x36,0x00,0x3e,0x63,0x7f,0x63,0x00,  /* 0xc2 */
    0x6e,0x3b,0x00,0x3e,0x63,0x7f,0x63,0x00,  /* 0xc3 */
    0x63,0x1c,0x36,0x63,0x7f,0x63,0x63,0x00,  /* 0xc4 */
    0x0c,0x0c,0x00,0x1e,0x33,0x3f,0x33,0x00,  /* 0xc5 */
    0x7c,0x36,0x33,0x7f,0x33,0x33,0x73,0x00,  /* 0xc6 */
    0x1e,0x33,0x03,0x33,0x1e,0x18,0x30,0x1e,  /* 0xc7 */
    0x07,0x00,0x3f,0x06,0x1e,0x06,0x3f,0x00,  /* 0xc8 */
    0x38,0x00,0x3f,0x06,0x1e,0x06,0x3f,0x00,  /* 0xc9 */
    0x0c,0x12,0x3f,0x06,0x1e,0x06,0x3f,0x00,  /* 0xca */
    0x36,0x00,0x3f,0x06,0x1e,0x06,0x3f,0x00,  /* 0xcb */
    0x07,0x00,0x1e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xcc */
    0x38,0x00,0x1e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xcd */
    0x0c,0x12,0x00,0x1e,0x0c,0x0c,0x1e,0x00,  /* 0xce */
    0x33,0x00,0x1e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xcf */
    0x3f,0x66,0x6f,0x6f,0x66,0x66,0x3f,0x00,  /* 0xd0 */
    0x3f,0x00,0x33,0x37,0x3f,0x3b,0x33,0x00,  /* 0xd1 */
    0x0e,0x00,0x18,0x3c,0x66,0x3c,0x18,0x00,  /* 0xd2 */
    0x70,0x00,0x18,0x3c,0x66,0x3c,0x18,0x00,  /* 0xd3 */
    0x3c,0x66,0x18,0x3c,0x66,0x3c,0x18,0x00,  /* 0xd4 */
    0x6e,0x3b,0x00,0x3e,0x63,0x63,0x3e,0x00,  /* 0xd5 */
    0xc3,0x18,0x3c,0x66,0x66,0x3c,0x18,0x00,  /* 0xd6 */
    0x00,0x36,0x1c,0x08,0x1c,0x36,0x00,0x00,  /* 0xd7 */
    0x5c,0x36,0x73,0x7b,0x6f,0x36,0x1d,0x00,  /* 0xd8 */
    0x0e,0x00,0x66,0x66,0x66,0x66,0x3c,0x00,  /* 0xd9 */
    0x70,0x00,0x66,0x66,0x66,0x66,0x3c,0x00,  /* 0xda */
    0x3c,0x66,0x00,0x66,0x66,0x66,0x3c,0x00,  /* 0xdb */
    0x33,0x00,0x33,0x33,0x33,0x33,0x1e,0x00,  /* 0xdc */
    0x70,0x00,0x66,0x66,0x3c,0x18,0x18,0x00,  /* 0xdd */
    0x0f,0x06,0x3e,0x66,0x66,0x3e,0x06,0x0f,  /* 0xde */
    0x00,0x1e,0x33,0x1f,0x33,0x1f,0x03,0x03,  /* 0xdf */
    0x07,0x00,0x1e,0x30,0x3e,0x33,0x7e,0x00,  /* 0xe0 */
    0x38,0x00,0x1e,0x30,0x3e,0x33,0x7e,0x00,  /* 0xe1 */
    0x7e,0xc3,0x3c,0x60,0x7c,0x66,0xfc,0x00,  /* 0xe2 */
    0x6e,0x3b,0x1e,0x30,0x3e,0x33,0x7e,0x00,  /* 0xe3 */
    0x33,0x00,0x1e,0x30,0x3e,0x33,0x7e,0x00,  /* 0xe4 */
    0x0c,0x0c,0x1e,0x30,0x3e,0x33,0x7e,0x00,  /* 0xe5 */
    0x00,0x00,0xfe,0x30,0xfe,0x33,0xfe,0x00,  /* 0xe6 */
    0x00,0x00,0x1e,0x03,0x03,0x1e,0x30,0x1c,  /* 0xe7 */
    0x07,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00,  /* 0xe8 */
    0x38,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00,  /* 0xe9 */
    0x7e,0xc3,0x3c,0x66,0x7e,0x06,0x3c,0x00,  /* 0xea */
    0x33,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00,  /* 0xeb */
    0x07,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xec */
    0x1c,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xed */
    0x3e,0x63,0x1c,0x18,0x18,0x18,0x3c,0x00,  /* 0xee */
    0x33,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00,  /* 0xef */
    0x1b,0x0e,0x1b,0x30,0x3e,0x33,0x1e,0x00,  /* 0xf0 */
    0x00,0x1f,0x00,0x1f,0x33,0x33,0x33,0x00,  /* 0xf1 */
    0x00,0x07,0x00,0x1e,0x33,0x33,0x1e,0x00,  /* 0xf2 */
    0x00,0x38,0x00,0x1e,0x33,0x33,0x1e,0x00,  /* 0xf3 */
    0x1e,0x33,0x00,0x1e,0x33,0x33,0x1e,0x00,  /* 0xf4 */
    0x6e,0x3b,0x00,0x1e,0x33,0x33,0x1e,0x00,  /* 0xf5 */
    0x00,0x33,0x00,0x1e,0x33,0x33,0x1e,0x00,  /* 0xf6 */
    0x18,0x18,0x00,0x7e,0x00,0x18,0x18,0x00,  /* 0xf7 */
    0x00,0x60,0x3c,0x76,0x7e,0x6e,0x3c,0x06,  /* 0xf8 */
    0x00,0x07,0x00,0x33,0x33,0x33,0x7e,0x00,  /* 0xf9 */
    0x00,0x38,0x00,0x33,0x33,0x33,0x7e,0x00,  /* 0xfa */
    0x1e,0x33,0x00,0x33,0x33,0x33,0x7e,0x00,  /* 0xfb */
    0x00,0x33,0x00,0x33,0x33,0x33,0x7e,0x00,  /* 0xfc */
    0x00,0x38,0x00,0x33,0x33,0x3e,0x30,0x1f,  /* 0xfd */
    0x00,0x00,0x06,0x3e,0x66,0x3e,0x06,0x00,  /* 0xfe */
    0x00,0x33,0x00,0x33,0x33,0x3e,0x30,0x1f,  /* 0xff */
    0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,  /* 0x100 */
};

static bool __sdl_debug_atlas_create(SDL_Renderer *renderer) {
    const int cw = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    const int ch = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    /* each glyph gets a 1px transparent border (upstream: "two pixels
       taller/wider, to prevent scaling artifacts") */
    const int rows = (__SDL_DEBUG_FONT_NUM_GLYPHS / __SDL_DEBUG_FONT_GLYPHS_PER_ROW) + 1;
    const int aw = (cw + 2) * __SDL_DEBUG_FONT_GLYPHS_PER_ROW;
    const int ah = rows * (ch + 2);
    Uint32 *px = (Uint32 *)calloc((size_t)aw * (size_t)ah, sizeof(Uint32));
    if (!px) return SDL_SetError("Out of memory");
    int column = 0, row = 0;
    for (int glyph = 0; glyph < __SDL_DEBUG_FONT_NUM_GLYPHS; glyph++) {
        const Uint8 *charpos = __sdl_debug_font_data + glyph * 8;
        Uint32 *linepos = px + (row * (ch + 2) + 1) * aw + column * (cw + 2) + 1;
        for (int iy = 0; iy < ch; iy++) {
            for (int ix = 0; ix < cw; ix++)
                if (charpos[iy] & (1 << ix)) linepos[ix] = 0xffffffffu;
            linepos += aw;
        }
        if (++column >= __SDL_DEBUG_FONT_GLYPHS_PER_ROW) { row++; column = 0; }
    }
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, aw, ah);
    if (!t) { free(px); return 0; }
    if (!SDL_UpdateTexture(t, NULL, px, aw * 4)) { free(px); SDL_DestroyTexture(t); return 0; }
    free(px);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    renderer->debug_atlas = t;
    return 1;
}

/* Codepoint -> atlas cell, upstream DrawDebugCharacter verbatim. */
static bool __sdl_debug_draw_char(SDL_Renderer *renderer, float x, float y, Uint32 c) {
    const int cw = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    const int ch = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    Uint32 ci = c;
    if ((ci <= 32) || ((ci >= 127) && (ci <= 160))) {
        return 1;  /* blank chars: draw nothing, caller still advances */
    } else if (ci >= __SDL_DEBUG_FONT_NUM_GLYPHS) {
        ci = __SDL_DEBUG_FONT_NUM_GLYPHS - 1;  /* the "invalid" glyph */
    } else if (ci < 127) {
        ci -= 33;   /* the 33 blank glyphs at the start */
    } else {
        ci -= 67;   /* + the 34-codepoint gap in the middle */
    }
    SDL_FRect srect = {
        (float)((ci % __SDL_DEBUG_FONT_GLYPHS_PER_ROW) * (cw + 2) + 1),
        (float)((ci / __SDL_DEBUG_FONT_GLYPHS_PER_ROW) * (ch + 2) + 1),
        (float)cw, (float)ch };
    SDL_FRect drect = { x, y, (float)cw, (float)ch };
    return SDL_RenderTexture(renderer, renderer->debug_atlas, &srect, &drect);
}

/* Decode one UTF-8 codepoint at *p (advancing, never past end). Invalid,
   truncated, overlong, surrogate or out-of-range sequences decode to U+FFFD
   consuming one byte — the SDL_StepUTF8 rule (ttf_step_utf8 in __SDL_ttf.c
   is the same rule; separate TU, so a static twin lives here). U+FFFD then
   lands in the >= NUM_GLYPHS branch above: bad input renders as the
   "invalid" glyph instead of as garbage. */
static Uint32 __sdl_step_utf8(const char **p, const char *end) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    Uint32 cp;
    int len, i;
    if (c < 0x80) { *p += 1; return c; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
    else { *p += 1; return 0xFFFD; }
    if (*p + len > end) { *p += 1; return 0xFFFD; }
    for (i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return 0xFFFD; }
        cp = (cp << 6) | (Uint32)(s[i] & 0x3F);
    }
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)
        || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) { *p += len; return 0xFFFD; }
    *p += len;
    return cp;
}

bool SDL_RenderDebugText(SDL_Renderer *renderer, float x, float y, const char *str) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!str) return SDL_InvalidParamError("str");
    if (!renderer->debug_atlas && !__sdl_debug_atlas_create(renderer)) return 0;
    bool result = 1;
    Uint8 r, g, b, a;
    result &= SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    result &= SDL_SetTextureColorMod(renderer->debug_atlas, r, g, b);
    result &= SDL_SetTextureAlphaMod(renderer->debug_atlas, a);
    const char *p = str;
    const char *end = str + strlen(str);
    float curx = x;
    while (result && p < end) {
        Uint32 ch = __sdl_step_utf8(&p, end);
        result &= __sdl_debug_draw_char(renderer, curx, y, ch);
        curx += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    }
    return result;
}

bool SDL_RenderDebugTextFormat(SDL_Renderer *renderer, float x, float y, const char *fmt, ...) {
    if (!__sdl_renderer_live(renderer)) return SDL_InvalidParamError("renderer");
    if (!fmt) return SDL_InvalidParamError("fmt");
    /* Format through libc vsnprintf — SDL and libc share one heap here, and
       upstream's "%s" fast path is an optimization, not contract. */
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return SDL_SetError("SDL_RenderDebugTextFormat: bad format string"); }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return SDL_SetError("Out of memory"); }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    bool ok = SDL_RenderDebugText(renderer, x, y, buf);
    free(buf);
    return ok;
}

void __setAnimationFrameFunc(void (*callback)(void)) {
    __sdl_set_animation_frame_func(callback);
}
  