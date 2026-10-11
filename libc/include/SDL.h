
#pragma once
/* SDL3 subset (video + events + audio) for this compiler's web backend.
   The functional surface matches the old SDL2 subset; the spelling is SDL3:
   flat keyboard event (scancode/key, no nested keysym), float mouse coords,
   4-arg SDL_CreateWindow, SDL_AudioStream push API, bool returns, Uint64 ticks.
   host.js's primitive __sdl_* imports are unchanged — __SDL.c maps SDL3 onto
   them. Legacy SDL2 source must be updated (see vendor/{doom,quake,...}). */
__require_source("__SDL.c");
#include <SDL3/SDL_intrin.h>

/* Real SDL.h pulls in <stddef.h> (via SDL_stdinc.h) so NULL/size_t are
   available to any program that only #includes <SDL.h> — match that. */
#include <stddef.h>

/* SDL3 uses C bool. Provide the type WITHOUT pulling in <stdbool.h>'s
   true/false macros, which would collide with code that defines its own
   (e.g. doomgeneric's 'typedef enum { false, true } boolean'). */
#ifndef __cplusplus
typedef _Bool bool;
#endif

typedef unsigned long long Uint64;
typedef unsigned int Uint32;
typedef unsigned short Uint16;
typedef unsigned char Uint8;
typedef long long Sint64;
typedef int Sint32;
typedef short Sint16;   /* SDL_AUDIO_S16 samples, gamepad axis values (#607) */
typedef signed char Sint8;   /* SDL_AUDIO_S8 samples (#707) */

#include <__SDL_foundation.h>

typedef Uint32 SDL_EventType;
typedef Uint32 SDL_AudioDeviceID;
typedef Uint64 SDL_TouchID;
typedef Uint64 SDL_FingerID;
typedef Uint32 SDL_WindowID;
typedef Uint32 SDL_KeyboardID;
typedef Uint32 SDL_MouseID;
typedef Uint32 SDL_JoystickID;   /* gamepad instance id (#607); 0 is invalid */
typedef Uint32 SDL_Scancode;
typedef Uint32 SDL_Keycode;
typedef Uint16 SDL_Keymod;

/* SDL3 made SDL_Surface a public struct. Match its field order + types so a
   program that reads surface->format / ->flags compiles and lays out the same.
   (This runtime only ever hands back RGBA32 window surfaces; refcount/reserved
   are inert here but present for layout fidelity.) */
typedef Uint32 SDL_PixelFormat;
typedef Uint32 SDL_SurfaceFlags;
typedef struct SDL_Surface {
    SDL_SurfaceFlags flags;
    SDL_PixelFormat format;
    int w, h;
    int pitch;
    void *pixels;
    int refcount;
    void *reserved;
} SDL_Surface;

/* Veneer-internal sentinel in SDL_Surface.flags: a heap surface this runtime
   OWNS (its pixels + the struct were malloc'd — e.g. by IMG_Load). Window
   surfaces (SDL_GetWindowSurface) carry flags==0 and are NOT heap, so
   SDL_DestroySurface frees iff this bit is set. A high bit keeps it clear of
   real SDL_SurfaceFlags (SDL_SURFACE_PREALLOCATED=1, ...). Shared by __SDL.c
   (SDL_DestroySurface), __SDL_image.c (IMG_Load) and __SDL_ttf.c
   (TTF_Render*). */
#define IMG_SURFACE_OWNED 0x80000000u

typedef struct SDL_Window SDL_Window;

typedef struct SDL_Rect {
    int x, y, w, h;
} SDL_Rect;

#include <__SDL_rect.h>

typedef struct SDL_FRect {
    float x, y, w, h;
} SDL_FRect;

typedef struct SDL_FPoint { float x, y; } SDL_FPoint;
typedef struct SDL_FColor { float r, g, b, a; } SDL_FColor;
/* SDL_Color: 8-bit-per-channel color (SDL_pixels.h). Used by-value across the
   SDL_ttf veneer's render calls, exactly as upstream spells them. */
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;

/* SDL_RenderGeometry vertex: position (render pixels), color (0..1 floats), and
   normalized (0..1) tex coords. */
typedef struct SDL_Vertex {
    SDL_FPoint position;
    SDL_FColor color;
    SDL_FPoint tex_coord;
} SDL_Vertex;

/* SDL_Renderer (2D accelerated). The host renders batched quads on WebGPU; the
   pixel format is informational only (textures are RGBA bytes in memory). */
typedef struct SDL_Renderer SDL_Renderer;

/* SDL_PixelFormat is typedef'd above (with SDL_Surface). Byte-order packed RGBA
   formats; the host treats every texture as RGBA bytes, so the exact value is
   informational. SDL_PIXELFORMAT_RGBA32 is defined ONCE, below, to the
   little-endian-correct value (== ABGR8888) — no conflicting redefinition. */
#define SDL_PIXELFORMAT_RGBA8888 0x16462004u
#define SDL_PIXELFORMAT_ABGR8888 0x16762004u

/* Pixel-format bitfield accessors + classification (SDL_pixels.h). Just enough
   for SDL_ISPIXELFORMAT_ALPHA, which SDL_CreateTexture uses to pick SDL3's
   alpha-aware default blend mode. These are pure bit math on the packed format
   id, so they're correct for any SDL pixel format, not only the ones we ship. */
#define SDL_PIXELFLAG(X)   (((X) >> 28) & 0x0F)
#define SDL_PIXELTYPE(X)   (((X) >> 24) & 0x0F)
#define SDL_PIXELORDER(X)  (((X) >> 20) & 0x0F)
#define SDL_PIXELTYPE_PACKED8  4
#define SDL_PIXELTYPE_PACKED16 5
#define SDL_PIXELTYPE_PACKED32 6
#define SDL_PACKEDORDER_ARGB 3
#define SDL_PACKEDORDER_RGBA 4
#define SDL_PACKEDORDER_ABGR 7
#define SDL_PACKEDORDER_BGRA 8
/* FOURCC formats carry flag != 1; the named RGBA formats all carry flag == 1. */
#define SDL_ISPIXELFORMAT_FOURCC(format) ((format) && (SDL_PIXELFLAG(format) != 1))
#define SDL_ISPIXELFORMAT_PACKED(format)     (!SDL_ISPIXELFORMAT_FOURCC(format) &&      ((SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED8) ||       (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED16) ||       (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32)))
#define SDL_ISPIXELFORMAT_ALPHA(format)     (SDL_ISPIXELFORMAT_PACKED(format) &&      ((SDL_PIXELORDER(format) == SDL_PACKEDORDER_ARGB) ||       (SDL_PIXELORDER(format) == SDL_PACKEDORDER_RGBA) ||       (SDL_PIXELORDER(format) == SDL_PACKEDORDER_ABGR) ||       (SDL_PIXELORDER(format) == SDL_PACKEDORDER_BGRA)))

typedef enum SDL_TextureAccess {
    SDL_TEXTUREACCESS_STATIC = 0,
    SDL_TEXTUREACCESS_STREAMING = 1,
    SDL_TEXTUREACCESS_TARGET = 2
} SDL_TextureAccess;

typedef enum SDL_BlendMode {
    SDL_BLENDMODE_NONE = 0,
    SDL_BLENDMODE_BLEND = 1,
    SDL_BLENDMODE_ADD = 2,
    SDL_BLENDMODE_MOD = 4
} SDL_BlendMode;

typedef enum SDL_ScaleMode {
    SDL_SCALEMODE_NEAREST = 0,
    SDL_SCALEMODE_LINEAR = 1
} SDL_ScaleMode;

/* Bitmask: HORIZONTAL|VERTICAL is a legal combination. */
typedef enum SDL_FlipMode {
    SDL_FLIP_NONE = 0,
    SDL_FLIP_HORIZONTAL = 1,
    SDL_FLIP_VERTICAL = 2
} SDL_FlipMode;

/* SDL3 SDL_Texture exposes format/w/h as public fields (programs read tex->w/h);
   the host handle is an internal trailer. */
typedef struct SDL_Texture {
    SDL_PixelFormat format;
    int w;
    int h;
    int __handle;
    Uint32 __magic;  /* liveness tag (#497): set at create, cleared at destroy,
                        checked by every texture-taking entry so use of a
                        destroyed texture fails with an SDL error instead of
                        silently "working". Best-effort (the check reads freed
                        memory once the allocator reuses it) — the same
                        magic-tag detection upstream SDL uses. */
    int __access;    /* SDL_TextureAccess at create (#496) — gates SDL_SetRenderTarget */
    SDL_Renderer *__renderer;  /* creating renderer (#496) — SDL_DestroyTexture
                        unbinds a still-bound target through it */
} SDL_Texture;

/* Fields common to the head of every SDL event (type/reserved/timestamp), shared
   layout so SDL_Event.common can read them for any event. */
typedef struct SDL_CommonEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
} SDL_CommonEvent;

/* SDL3 flattened the keyboard event: scancode/key live directly on the event
   (no SDL_Keysym sub-struct), timestamps are Uint64, and state is a bool down. */
typedef struct SDL_KeyboardEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    SDL_KeyboardID which;
    SDL_Scancode scancode;
    SDL_Keycode key;
    SDL_Keymod mod;
    Uint16 raw;
    bool down;
    bool repeat;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    SDL_MouseID which;
    Uint32 state;
    float x;
    float y;
    float xrel;
    float yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    SDL_MouseID which;
    Uint8 button;
    bool down;
    Uint8 clicks;
    Uint8 padding;
    float x;
    float y;
} SDL_MouseButtonEvent;

typedef struct SDL_MouseWheelEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    SDL_MouseID which;
    float x;
    float y;
    Uint32 direction;
    float mouse_x;
    float mouse_y;
} SDL_MouseWheelEvent;

/* SDL3 window event: data1/data2 are event-dependent (RESIZED: new w/h). */
typedef struct SDL_WindowEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    Sint32 data1;
    Sint32 data2;
} SDL_WindowEvent;

/* App-defined events (#604): SDL_RegisterEvents hands out types in the
   [SDL_EVENT_USER, SDL_EVENT_LAST] range and SDL_UserEvent is the payload
   shape a SDL_PushEvent caller fills in (code/data1/data2 are the app's). */
typedef struct SDL_UserEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    Sint32 code;
    void *data1;
    void *data2;
} SDL_UserEvent;

/* Gamepad events (#607) — field-for-field upstream SDL3 layouts. */
typedef struct SDL_GamepadDeviceEvent {
    Uint32 type;                /* SDL_EVENT_GAMEPAD_ADDED / _REMOVED */
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
} SDL_GamepadDeviceEvent;

typedef struct SDL_GamepadButtonEvent {
    Uint32 type;                /* SDL_EVENT_GAMEPAD_BUTTON_DOWN / _UP */
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 button;               /* SDL_GamepadButton */
    bool down;
    Uint8 padding1;
    Uint8 padding2;
} SDL_GamepadButtonEvent;

typedef struct SDL_GamepadAxisEvent {
    Uint32 type;                /* SDL_EVENT_GAMEPAD_AXIS_MOTION */
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 axis;                 /* SDL_GamepadAxis */
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;               /* sticks -32768..32767, triggers 0..32767 */
    Uint16 padding4;
} SDL_GamepadAxisEvent;

typedef struct SDL_TextInputEvent {Uint32 type;Uint32 reserved;Uint64 timestamp;SDL_WindowID windowID;const char *text;} SDL_TextInputEvent;

typedef struct SDL_TextEditingEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    const char *text;
    Sint32 start;
    Sint32 length;
} SDL_TextEditingEvent;
typedef struct SDL_DropEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_WindowID windowID;
    float x;
    float y;
    const char *source;
    const char *data;
} SDL_DropEvent;
typedef struct SDL_TouchFingerEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_TouchID touchID;
    SDL_FingerID fingerID;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
    SDL_WindowID windowID;
} SDL_TouchFingerEvent;
typedef struct SDL_AudioDeviceEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_AudioDeviceID which;
    bool recording;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
} SDL_AudioDeviceEvent;
typedef struct SDL_JoyAxisEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_JoyAxisEvent;
typedef struct SDL_JoyBallEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 ball;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 xrel;
    Sint16 yrel;
} SDL_JoyBallEvent;
typedef struct SDL_JoyHatEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 hat;
    Uint8 value;
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyHatEvent;
typedef struct SDL_JoyButtonEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Uint8 button;
    bool down;
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyButtonEvent;
typedef struct SDL_JoyDeviceEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
} SDL_JoyDeviceEvent;
typedef struct SDL_GamepadTouchpadEvent
{
    SDL_EventType type;
    Uint32 reserved;
    Uint64 timestamp;
    SDL_JoystickID which;
    Sint32 touchpad;
    Sint32 finger;
    float x;
    float y;
    float pressure;
} SDL_GamepadTouchpadEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_CommonEvent common;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_TextEditingEvent edit;
    SDL_DropEvent drop;
    SDL_TouchFingerEvent tfinger;
    SDL_AudioDeviceEvent adevice;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyBallEvent jball;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_JoyDeviceEvent jdevice;
    SDL_GamepadTouchpadEvent gtouchpad;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_WindowEvent window;
    SDL_GamepadDeviceEvent gdevice;
    SDL_GamepadButtonEvent gbutton;
    SDL_GamepadAxisEvent gaxis;
    SDL_UserEvent user;
    Uint8 padding[128];
} SDL_Event;

typedef Uint32 SDL_InitFlags;
typedef Uint64 SDL_WindowFlags;
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_HAPTIC 0x00001000u
#define SDL_INIT_GAMEPAD 0x00002000u
#define SDL_INIT_EVENTS 0x00004000u
#define SDL_INIT_SENSOR 0x00008000u
#define SDL_INIT_CAMERA 0x00010000u
#define SDL_WINDOW_FULLSCREEN 0x0000000000000001ULL
#define SDL_WINDOW_HIDDEN 0x0000000000000008ULL
#define SDL_WINDOW_MINIMIZED 0x0000000000000040ULL
#define SDL_WINDOW_INPUT_FOCUS 0x0000000000000200ULL
/* Borderless: under the OS WM this is a kernel surface with no chrome
   (taskbar-class windows — docs/archive/0014); standalone runtimes ignore it. */
#define SDL_WINDOW_BORDERLESS 0x0000000000000010ULL
/* Resizable: declared by apps that handle SDL_EVENT_WINDOW_RESIZED (the
   0019 renegotiation). Accepted everywhere; docs/archive/0021 will make the WM
   offer resize ONLY to windows that declare it. */
#define SDL_WINDOW_RESIZABLE 0x0000000000000020ULL
/* Transparent (SDL3 value): under the OS WM the surface's per-pixel alpha
   is honored — the compositor blends it src-over (docs/archive/0063). Standalone
   runtimes ignore it (the page canvas is opaque). */
#define SDL_WINDOW_TRANSPARENT 0x0000000040000000ULL
/* Utility (SDL3 value): a transient/owned window that does NOT appear in the
   taskbar — under the OS WM the surface is marked transient (kernel flag bit4,
   docs/archive/0281) so /bin/wm gives it no taskbar button and skips it when cycling
   (owned modals — MessageBox, dialogs — are never taskbar entries in Win95).
   Standalone runtimes ignore it. */
#define SDL_WINDOW_UTILITY 0x0000000000020000ULL
/* Popup windows (SDL3 values; docs/archive/0256): created via SDL_CreatePopupWindow
   as kernel ANCHORED CHILD surfaces — borderless, pinned to their parent at
   a fixed offset, moved/hidden/raised/destroyed/scaled with it, never
   focused. POPUP_MENU additionally holds the kernel GRAB while it lives: a
   press outside its window tree dismisses it (the popup's window gets
   SDL_EVENT_WINDOW_CLOSE_REQUESTED) and the press is consumed — Win95
   menu-mode capture. TOOLTIP does not grab. */
#define SDL_WINDOW_TOOLTIP 0x0000000000040000ULL
#define SDL_WINDOW_POPUP_MENU 0x0000000000080000ULL
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
/* SDL3: on little-endian, SDL_PIXELFORMAT_RGBA32 aliases ABGR8888
   (0x16762004 = 376840196). Single canonical definition. */
#define SDL_PIXELFORMAT_RGBA32 0x16762004u
#define SDL_PIXELFORMAT_XRGB8888 0x16161804u

/* SDL3 event types (SDL_EVENT_*). Values are unchanged from SDL2. */
#define SDL_EVENT_QUIT 0x100
/* SDL3 window events (flattened enum, one type per event). */
#define SDL_EVENT_WINDOW_SHOWN 0x202
#define SDL_EVENT_WINDOW_HIDDEN 0x203
#define SDL_EVENT_WINDOW_EXPOSED 0x204
#define SDL_EVENT_WINDOW_MOVED 0x205
#define SDL_EVENT_WINDOW_RESIZED 0x206
#define SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED 0x207
#define SDL_EVENT_WINDOW_MOUSE_ENTER 0x20C
#define SDL_EVENT_WINDOW_MOUSE_LEAVE 0x20D
/* The owner focus pair (docs/archive/0256): the kernel emits these at EVERY focus
   transition — another window's create-steal, a click/WM focus, and the
   focus fall after a destroy/minimize — via its single focus choke point. */
#define SDL_EVENT_WINDOW_FOCUS_GAINED 0x20E
#define SDL_EVENT_WINDOW_FOCUS_LOST 0x20F
/* Delivered when the kernel's close request ('x' / wmctl close) names a
   window and OTHER windows are still live (docs/archive/0089) — a multi-window
   process closes just that window. A single (or last) window keeps the
   process-wide SDL_EVENT_QUIT it always got. */
#define SDL_EVENT_WINDOW_CLOSE_REQUESTED 0x210
#define SDL_EVENT_KEY_DOWN 0x300
#define SDL_EVENT_KEY_UP 0x301
#define SDL_EVENT_MOUSE_MOTION 0x400
#define SDL_EVENT_MOUSE_BUTTON_DOWN 0x401
#define SDL_EVENT_MOUSE_BUTTON_UP 0x402
#define SDL_EVENT_MOUSE_WHEEL 0x403
/* Gamepad events (#607). The five delivered kinds — this runtime has no
   touchpad/sensor/remap sources, so those constants are deliberately not
   declared (absence is honest, the API-honesty rule). */
#define SDL_EVENT_GAMEPAD_AXIS_MOTION 0x650
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN 0x651
#define SDL_EVENT_GAMEPAD_BUTTON_UP 0x652
#define SDL_EVENT_GAMEPAD_ADDED 0x653
#define SDL_EVENT_GAMEPAD_REMOVED 0x654
/* App-registered event range (#604): SDL_RegisterEvents allocates from
   SDL_EVENT_USER upward; SDL_EVENT_LAST bounds the legal type space. */
#define SDL_EVENT_FIRST 0x0
#define SDL_EVENT_USER 0x8000
#define SDL_EVENT_LAST 0xFFFF
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3

#define SDLK_BACKSPACE 8
#define SDLK_TAB 9
#define SDLK_RETURN 13
#define SDLK_ESCAPE 27
#define SDLK_SPACE 32
#define SDLK_PLUS 43
#define SDLK_MINUS 45
#define SDLK_EQUALS 61
#define SDLK_DELETE 127
#define SDLK_CAPSLOCK 1073741881
#define SDLK_F1 1073741882
#define SDLK_F2 1073741883
#define SDLK_F3 1073741884
#define SDLK_F4 1073741885
#define SDLK_F5 1073741886
#define SDLK_F6 1073741887
#define SDLK_F7 1073741888
#define SDLK_F8 1073741889
#define SDLK_F9 1073741890
#define SDLK_F10 1073741891
#define SDLK_F11 1073741892
#define SDLK_F12 1073741893
#define SDLK_PRINTSCREEN 1073741894
#define SDLK_SCROLLLOCK 1073741895
#define SDLK_PAUSE 1073741896
#define SDLK_INSERT 1073741897
#define SDLK_HOME 1073741898
#define SDLK_PAGEUP 1073741899
#define SDLK_END 1073741901
#define SDLK_PAGEDOWN 1073741902
#define SDLK_RIGHT 1073741903
#define SDLK_LEFT 1073741904
#define SDLK_DOWN 1073741905
#define SDLK_UP 1073741906
#define SDLK_NUMLOCKCLEAR 1073741907
#define SDLK_LCTRL 1073742048
#define SDLK_LSHIFT 1073742049
#define SDLK_LALT 1073742050
#define SDLK_LGUI 1073742051
#define SDLK_RCTRL 1073742052
#define SDLK_RSHIFT 1073742053
#define SDLK_RALT 1073742054
#define SDLK_RGUI 1073742055

/* SDL3 key modifier flags (SDL_Keymod, SDL_keycode.h). Populated on
   event.key.mod from the DOM modifier state. */
#define SDL_KMOD_NONE   0x0000
#define SDL_KMOD_LSHIFT 0x0001
#define SDL_KMOD_RSHIFT 0x0002
#define SDL_KMOD_LCTRL  0x0040
#define SDL_KMOD_RCTRL  0x0080
#define SDL_KMOD_LALT   0x0100
#define SDL_KMOD_RALT   0x0200
#define SDL_KMOD_LGUI   0x0400
#define SDL_KMOD_RGUI   0x0800
#define SDL_KMOD_NUM    0x1000
#define SDL_KMOD_CAPS   0x2000
#define SDL_KMOD_MODE   0x4000
#define SDL_KMOD_SCROLL 0x8000
#define SDL_KMOD_CTRL  (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)
#define SDL_KMOD_SHIFT (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)
#define SDL_KMOD_ALT   (SDL_KMOD_LALT | SDL_KMOD_RALT)
#define SDL_KMOD_GUI   (SDL_KMOD_LGUI | SDL_KMOD_RGUI)

/* ---- SDL_Scancode (SDL_scancode.h) — the physical-key table ----
   SDL3 flattened the keyboard event to carry a scancode directly
   (event.key.scancode). These are the full USB-HID usage-page numbers — the
   SAME values host.js maps DOM KeyboardEvent.code onto (SCANCODE_MAP), so a
   C-side SDL_SCANCODE_* compares equal to what the host delivers. This is the
   complete standard table, not just one app's keys: a physical-key table is a
   fixed ABI, so ship it whole. */
enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B = 5, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8, SDL_SCANCODE_F = 9, SDL_SCANCODE_G = 10, SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12, SDL_SCANCODE_J = 13, SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16, SDL_SCANCODE_N = 17, SDL_SCANCODE_O = 18, SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20, SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24, SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28, SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32, SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34, SDL_SCANCODE_6 = 35, SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43, SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_MINUS = 45, SDL_SCANCODE_EQUALS = 46, SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48, SDL_SCANCODE_BACKSLASH = 49, SDL_SCANCODE_NONUSHASH = 50,
    SDL_SCANCODE_SEMICOLON = 51, SDL_SCANCODE_APOSTROPHE = 52, SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54, SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56,
    SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60, SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62, SDL_SCANCODE_F6 = 63, SDL_SCANCODE_F7 = 64, SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66, SDL_SCANCODE_F10 = 67, SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_PRINTSCREEN = 70, SDL_SCANCODE_SCROLLLOCK = 71, SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_NUMLOCKCLEAR = 83, SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86, SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89, SDL_SCANCODE_KP_2 = 90, SDL_SCANCODE_KP_3 = 91, SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93, SDL_SCANCODE_KP_6 = 94, SDL_SCANCODE_KP_7 = 95, SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97, SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_PERIOD = 99,
    SDL_SCANCODE_NONUSBACKSLASH = 100, SDL_SCANCODE_APPLICATION = 101, SDL_SCANCODE_POWER = 102,
    SDL_SCANCODE_KP_EQUALS = 103,
    SDL_SCANCODE_F13 = 104, SDL_SCANCODE_F14 = 105, SDL_SCANCODE_F15 = 106, SDL_SCANCODE_F16 = 107,
    SDL_SCANCODE_F17 = 108, SDL_SCANCODE_F18 = 109, SDL_SCANCODE_F19 = 110, SDL_SCANCODE_F20 = 111,
    SDL_SCANCODE_F21 = 112, SDL_SCANCODE_F22 = 113, SDL_SCANCODE_F23 = 114, SDL_SCANCODE_F24 = 115,
    SDL_SCANCODE_EXECUTE = 116, SDL_SCANCODE_HELP = 117, SDL_SCANCODE_MENU = 118,
    SDL_SCANCODE_SELECT = 119, SDL_SCANCODE_STOP = 120, SDL_SCANCODE_AGAIN = 121,
    SDL_SCANCODE_UNDO = 122, SDL_SCANCODE_CUT = 123, SDL_SCANCODE_COPY = 124,
    SDL_SCANCODE_PASTE = 125, SDL_SCANCODE_FIND = 126, SDL_SCANCODE_MUTE = 127,
    SDL_SCANCODE_VOLUMEUP = 128, SDL_SCANCODE_VOLUMEDOWN = 129,
    SDL_SCANCODE_KP_COMMA = 133, SDL_SCANCODE_KP_EQUALSAS400 = 134,
    SDL_SCANCODE_INTERNATIONAL1 = 135, SDL_SCANCODE_INTERNATIONAL2 = 136,
    SDL_SCANCODE_INTERNATIONAL3 = 137, SDL_SCANCODE_INTERNATIONAL4 = 138,
    SDL_SCANCODE_INTERNATIONAL5 = 139, SDL_SCANCODE_INTERNATIONAL6 = 140,
    SDL_SCANCODE_INTERNATIONAL7 = 141, SDL_SCANCODE_INTERNATIONAL8 = 142,
    SDL_SCANCODE_INTERNATIONAL9 = 143,
    SDL_SCANCODE_LANG1 = 144, SDL_SCANCODE_LANG2 = 145, SDL_SCANCODE_LANG3 = 146,
    SDL_SCANCODE_LANG4 = 147, SDL_SCANCODE_LANG5 = 148, SDL_SCANCODE_LANG6 = 149,
    SDL_SCANCODE_LANG7 = 150, SDL_SCANCODE_LANG8 = 151, SDL_SCANCODE_LANG9 = 152,
    SDL_SCANCODE_ALTERASE = 153, SDL_SCANCODE_SYSREQ = 154, SDL_SCANCODE_CANCEL = 155,
    SDL_SCANCODE_CLEAR = 156, SDL_SCANCODE_PRIOR = 157, SDL_SCANCODE_RETURN2 = 158,
    SDL_SCANCODE_SEPARATOR = 159, SDL_SCANCODE_OUT = 160, SDL_SCANCODE_OPER = 161,
    SDL_SCANCODE_CLEARAGAIN = 162, SDL_SCANCODE_CRSEL = 163, SDL_SCANCODE_EXSEL = 164,
    SDL_SCANCODE_KP_00 = 176, SDL_SCANCODE_KP_000 = 177, SDL_SCANCODE_THOUSANDSSEPARATOR = 178,
    SDL_SCANCODE_DECIMALSEPARATOR = 179, SDL_SCANCODE_CURRENCYUNIT = 180,
    SDL_SCANCODE_CURRENCYSUBUNIT = 181, SDL_SCANCODE_KP_LEFTPAREN = 182,
    SDL_SCANCODE_KP_RIGHTPAREN = 183, SDL_SCANCODE_KP_LEFTBRACE = 184,
    SDL_SCANCODE_KP_RIGHTBRACE = 185, SDL_SCANCODE_KP_TAB = 186, SDL_SCANCODE_KP_BACKSPACE = 187,
    SDL_SCANCODE_KP_A = 188, SDL_SCANCODE_KP_B = 189, SDL_SCANCODE_KP_C = 190,
    SDL_SCANCODE_KP_D = 191, SDL_SCANCODE_KP_E = 192, SDL_SCANCODE_KP_F = 193,
    SDL_SCANCODE_KP_XOR = 194, SDL_SCANCODE_KP_POWER = 195, SDL_SCANCODE_KP_PERCENT = 196,
    SDL_SCANCODE_KP_LESS = 197, SDL_SCANCODE_KP_GREATER = 198, SDL_SCANCODE_KP_AMPERSAND = 199,
    SDL_SCANCODE_KP_DBLAMPERSAND = 200, SDL_SCANCODE_KP_VERTICALBAR = 201,
    SDL_SCANCODE_KP_DBLVERTICALBAR = 202, SDL_SCANCODE_KP_COLON = 203, SDL_SCANCODE_KP_HASH = 204,
    SDL_SCANCODE_KP_SPACE = 205, SDL_SCANCODE_KP_AT = 206, SDL_SCANCODE_KP_EXCLAM = 207,
    SDL_SCANCODE_KP_MEMSTORE = 208, SDL_SCANCODE_KP_MEMRECALL = 209, SDL_SCANCODE_KP_MEMCLEAR = 210,
    SDL_SCANCODE_KP_MEMADD = 211, SDL_SCANCODE_KP_MEMSUBTRACT = 212, SDL_SCANCODE_KP_MEMMULTIPLY = 213,
    SDL_SCANCODE_KP_MEMDIVIDE = 214, SDL_SCANCODE_KP_PLUSMINUS = 215, SDL_SCANCODE_KP_CLEAR = 216,
    SDL_SCANCODE_KP_CLEARENTRY = 217, SDL_SCANCODE_KP_BINARY = 218, SDL_SCANCODE_KP_OCTAL = 219,
    SDL_SCANCODE_KP_DECIMAL = 220, SDL_SCANCODE_KP_HEXADECIMAL = 221,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_LGUI = 227, SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230, SDL_SCANCODE_RGUI = 231,
    SDL_SCANCODE_MODE = 257,
    SDL_SCANCODE_COUNT = 512
};

/* SDL_MouseWheelDirection */
#define SDL_MOUSEWHEEL_NORMAL  0
#define SDL_MOUSEWHEEL_FLIPPED 1

/* SDL button mask helper (SDL_mouse.h): SDL_BUTTON_MASK(b) == 1u << (b-1). */
#define SDL_BUTTON_MASK(X) (1u << ((X) - 1))
#define SDL_BUTTON_LMASK SDL_BUTTON_MASK(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)
typedef Uint32 SDL_MouseButtonFlags;

/* SDL3 audio format constants (SDL_AUDIO_*). Values are unchanged from SDL2. */
typedef int SDL_AudioFormat;
#define SDL_AUDIO_U8 0x0008
#define SDL_AUDIO_S8 0x8008
#define SDL_AUDIO_S16 0x8010
#define SDL_AUDIO_S32 0x8020
#define SDL_AUDIO_F32 0x8120

#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0xFFFFFFFFu
#define SDL_AUDIO_DEVICE_DEFAULT_RECORDING 0xFFFFFFFEu

/* SDL3 reordered SDL_AudioSpec to {format, channels, freq} and dropped the
   SDL2 callback/samples/silence/size fields. */
typedef struct SDL_AudioSpec {
    SDL_AudioFormat format;
    int channels;
    int freq;
} SDL_AudioSpec;

typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (*SDL_AudioStreamCallback)(void *userdata, SDL_AudioStream *stream,
                                        int additional_amount, int total_amount);

bool SDL_Init(SDL_InitFlags flags);
bool SDL_InitSubSystem(SDL_InitFlags flags);
void SDL_QuitSubSystem(SDL_InitFlags flags);
SDL_InitFlags SDL_WasInit(SDL_InitFlags flags);
SDL_Window *SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags);
/* SDL_CreatePopupWindow / SDL_GetDisplayBounds (docs/archive/0256) live in
   <SDL_popup.h> — a subsidiary header on the sdl3webgpu.h precedent, so the
   popup TU (and its host imports) links only into binaries that use popups
   instead of growing every SDL binary's import table. */
SDL_WindowID SDL_GetWindowID(SDL_Window *window);
SDL_Surface *SDL_GetWindowSurface(SDL_Window *window);
bool SDL_UpdateWindowSurface(SDL_Window *window);
bool SDL_GetWindowSize(SDL_Window *window, int *w, int *h);
/* Geometry transforms (#790, SDL3 contracts kept exactly): gucOS windows
   are 1 buffer pixel = 1 screen pixel (no HiDPI virtualization — the screen
   tracks the viewport at 1 CSS px per screen px), so the size in pixels IS
   the window size, the pixel density is 1.0 and the display scale is 1.0.
   A WM-scaled fixed-size window (docs/archive/0024, SET_DST) is a presentation
   policy the app never sees: its buffer, and therefore every value here,
   stays the buffer geometry — input arrives inverse-mapped into it. */
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h);
float SDL_GetWindowPixelDensity(SDL_Window *window);
float SDL_GetWindowDisplayScale(SDL_Window *window);
bool SDL_SetWindowSize(SDL_Window *window, int w, int h);
/* gucOS queries current HIDDEN, INPUT_FOCUS and MINIMIZED state from the
   owning kernel. Other bits retain the declared creation flags. Standalone
   runtimes without this state query retain their create-time flags. */
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *window);
/* gucOS: showing preserves focus; raising separately requests WM activation.
   Runtimes without a window-system lifecycle fail with SDL_GetError(). */
bool SDL_ShowWindow(SDL_Window *window);
bool SDL_HideWindow(SDL_Window *window);
bool SDL_RaiseWindow(SDL_Window *window);
/* Owner relation (#794; SDL3 "parent" of a non-popup window): the window
   stacks above its parent, is hidden/minimized with it (its own hidden flag
   is preserved), and SDL_DestroyWindow(parent) destroys it recursively.
   KNOWN DEVIATION from upstream SDL3: upstream's SDL_HideWindow recursively
   hides parented children and SETS their SDL_WINDOW_HIDDEN (restoring on
   show); here SDL_WINDOW_HIDDEN stays the window's OWN requested state per
   the gucOS UI contract, and guc_window_viewable answers the effective
   question. A client that reads SDL_WINDOW_HIDDEN to mean "off screen"
   must query guc_window_viewable instead.
   Same-process only; a popup window (SDL_CreatePopupWindow) cannot change
   its parent and cannot be a parent. NULL clears. Refused loud on runtimes
   without the window-system lifecycle. */
bool SDL_SetWindowParent(SDL_Window *window, SDL_Window *parent);
SDL_Window *SDL_GetWindowParent(SDL_Window *window);
SDL_Window *SDL_GetWindowFromID(SDL_WindowID id);
/* gucOS extension (#794) — a custom name on purpose, not an SDL API: why the
   LAST SDL_EVENT_WINDOW_CLOSE_REQUESTED for this window was queued. A popup
   grab dismissal (press outside the popup's window tree) reaches the app as
   the same close event it always did, but it is UI housekeeping — it never
   arms the kernel's hung-app watchdog — and a toolkit that dismisses a menu
   must not treat it as a user close request. 0 before any close event. */
#define GUC_CLOSE_REASON_REQUEST 0
#define GUC_CLOSE_REASON_POPUP_DISMISS 1
int guc_window_close_reason(SDL_Window *window);
/* gucOS extension (#794): EFFECTIVE visibility — 1 when the window system
   composites and hit-tests the window (mapped, not minimized, no hidden or
   minimized parent), 0 when not, -1 with SDL_GetError() where there is no
   window system. SDL_WINDOW_HIDDEN is the window's OWN requested state; the
   two differ exactly while a parent is hidden or minimized. */
int guc_window_viewable(SDL_Window *window);
/* Window placement is the WM's job in gucOS, so SDL_SetWindowPosition is an
   honest accept-and-succeed no-op (a self-placing app would only fight the
   compositor). SDL_SetWindowIcon likewise succeeds without a taskbar-icon
   pipe yet — the game treats icon failure as fatal, so it MUST return true;
   the RGBA pixels are simply not consumed. Both keep the app source
   unmodified; both are drop-in upgradeable to real kernel hooks later. */
bool SDL_SetWindowPosition(SDL_Window *window, int x, int y);
bool SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
/* Free a heap surface this runtime owns (IMG_SURFACE_OWNED); a no-op on a
   window surface or NULL. */
void SDL_DestroySurface(SDL_Surface *surface);
#include <__SDL_events.h>
bool SDL_PollEvent(SDL_Event *event);
bool SDL_WaitEvent(SDL_Event *event);
bool SDL_WaitEventTimeout(SDL_Event *event, Sint32 timeoutMS);
/* ---- Event-queue manipulation (#604) ----
   Operations over the veneer's own queue. SDL_PumpEvents is the explicit
   drain of the OS input ring (the same primitive SDL_PollEvent pumps
   through), which also advances the #493 input-state snapshots — so the
   upstream SDL_PumpEvents + SDL_GetKeyboardState no-event-loop idiom reads
   exactly as documented. The scan/flush entries operate on the queue as-is
   (upstream semantics: they do not pump). SDL_PushEvent stamps a zero
   timestamp; this runtime is single-threaded, so a push can never race a
   parked SDL_WaitEvent — the wait loop re-polls before every park, and a
   pushed event completes the next wait without parking. */
void SDL_PumpEvents(void);
bool SDL_PushEvent(SDL_Event *event);
Uint32 SDL_RegisterEvents(int numevents);
bool SDL_HasEvent(Uint32 type);
bool SDL_HasEvents(Uint32 minType, Uint32 maxType);
void SDL_FlushEvent(Uint32 type);
void SDL_FlushEvents(Uint32 minType, Uint32 maxType);
typedef enum SDL_EventAction {
    SDL_ADDEVENT,
    SDL_PEEKEVENT,
    SDL_GETEVENT
} SDL_EventAction;
int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_EventAction action, Uint32 minType, Uint32 maxType);
void SDL_DestroyWindow(SDL_Window *window);
void SDL_Quit(void);
void SDL_Delay(Uint32 ms);
Uint64 SDL_GetTicks(void);
/* Sub-millisecond timing (SDL_timer.h; #495) — the frame-pacing / delta-time
   surface. All three ride the same monotonic host clock SDL_GetTicks
   truncates (fractional ms since SDL_Init, performance.now()-backed). The
   performance counter's unit is fixed at nanoseconds:
   SDL_GetPerformanceFrequency() == 1000000000, as on SDL3's POSIX
   clock_gettime backends. */
Uint64 SDL_GetTicksNS(void);
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
/* Input state snapshots (SDL_keyboard.h / SDL_mouse.h; #493) — the idiomatic
   per-frame game read. Updated as events are pumped (single keyboard/mouse in
   this runtime); the keyboard array is SDL_SCANCODE_COUNT entries, valid for
   the whole app lifetime. SDL_GetGlobalMouseState is declared but always
   FAILS (0 mask, 0,0, SDL error set): a gucOS process only sees pointer
   events routed to its own windows, so desktop-global cursor position is not
   knowable here — per the API-honesty rule it fails loud instead of lying
   with window-relative coordinates. */
const bool *SDL_GetKeyboardState(int *numkeys);
SDL_Keymod SDL_GetModState(void);
SDL_MouseButtonFlags SDL_GetMouseState(float *x, float *y);
SDL_MouseButtonFlags SDL_GetGlobalMouseState(float *x, float *y);

/* ---- Gamepads (SDL_gamepad.h; #607) ----
   The full SDL3 gamepad object model over the OS pad registry: instance
   ids, ADDED/REMOVED hotplug events, per-button/axis events and state.
   Requires SDL_Init(SDL_INIT_GAMEPAD). Pads route to the FOCUSED window's
   process (the keyboard rule); an unfocused process sees frozen state and
   is reconciled on focus gain. In the browser the backing device is the
   Web Gamepad API's "standard" mapping (the browser did the mapping —
   there is no mapping-DB API here, and pads the browser cannot map to the
   standard layout are not exposed). Enum values and event layouts are
   upstream SDL3, verbatim. Deliberately absent (honest boundary, not yet
   backed): the joystick-level API (SDL_INIT_JOYSTICK still fails loud —
   this runtime has no unmapped-device view, so unlike upstream,
   SDL_INIT_GAMEPAD does NOT imply it), rumble, touchpad/sensor/LED,
   mappings, player indices. */
typedef enum SDL_GamepadType {
    SDL_GAMEPAD_TYPE_UNKNOWN = 0,
    SDL_GAMEPAD_TYPE_STANDARD,      /* every pad here: the web standard mapping */
    SDL_GAMEPAD_TYPE_COUNT
} SDL_GamepadType;

typedef enum SDL_GamepadButton {
    SDL_GAMEPAD_BUTTON_INVALID = -1,
    SDL_GAMEPAD_BUTTON_SOUTH,           /* 0  */
    SDL_GAMEPAD_BUTTON_EAST,            /* 1  */
    SDL_GAMEPAD_BUTTON_WEST,            /* 2  */
    SDL_GAMEPAD_BUTTON_NORTH,           /* 3  */
    SDL_GAMEPAD_BUTTON_BACK,            /* 4  */
    SDL_GAMEPAD_BUTTON_GUIDE,           /* 5  */
    SDL_GAMEPAD_BUTTON_START,           /* 6  */
    SDL_GAMEPAD_BUTTON_LEFT_STICK,      /* 7  */
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,     /* 8  */
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,   /* 9  */
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,  /* 10 */
    SDL_GAMEPAD_BUTTON_DPAD_UP,         /* 11 */
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,       /* 12 */
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,       /* 13 */
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,      /* 14 */
    SDL_GAMEPAD_BUTTON_MISC1,           /* 15 — never delivered here */
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,   /* 16 — never delivered here */
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,    /* 17 — never delivered here */
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,   /* 18 — never delivered here */
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,    /* 19 — never delivered here */
    SDL_GAMEPAD_BUTTON_TOUCHPAD,        /* 20 — never delivered here */
    SDL_GAMEPAD_BUTTON_MISC2,           /* 21 — never delivered here */
    SDL_GAMEPAD_BUTTON_MISC3,           /* 22 — never delivered here */
    SDL_GAMEPAD_BUTTON_MISC4,           /* 23 — never delivered here */
    SDL_GAMEPAD_BUTTON_MISC5,           /* 24 — never delivered here */
    SDL_GAMEPAD_BUTTON_MISC6,           /* 25 — never delivered here */
    SDL_GAMEPAD_BUTTON_COUNT            /* 26, as upstream */
} SDL_GamepadButton;

typedef enum SDL_GamepadAxis {
    SDL_GAMEPAD_AXIS_INVALID = -1,
    SDL_GAMEPAD_AXIS_LEFTX,             /* 0 */
    SDL_GAMEPAD_AXIS_LEFTY,             /* 1 */
    SDL_GAMEPAD_AXIS_RIGHTX,            /* 2 */
    SDL_GAMEPAD_AXIS_RIGHTY,            /* 3 */
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER,      /* 4 — 0..32767, never negative */
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,     /* 5 — 0..32767, never negative */
    SDL_GAMEPAD_AXIS_COUNT
} SDL_GamepadAxis;

typedef struct SDL_Gamepad SDL_Gamepad;   /* opaque, as upstream */

SDL_JoystickID *SDL_GetGamepads(int *count);   /* 0-terminated; SDL_free() it */
bool SDL_HasGamepad(void);
bool SDL_IsGamepad(SDL_JoystickID instance_id);
SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID instance_id);
void SDL_CloseGamepad(SDL_Gamepad *gamepad);
SDL_Gamepad *SDL_GetGamepadFromID(SDL_JoystickID instance_id);
SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad *gamepad);
const char *SDL_GetGamepadName(SDL_Gamepad *gamepad);
const char *SDL_GetGamepadNameForID(SDL_JoystickID instance_id);
bool SDL_GamepadConnected(SDL_Gamepad *gamepad);
SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *gamepad);
bool SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);
bool SDL_GamepadHasButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);
bool SDL_GamepadHasAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);
void SDL_UpdateGamepads(void);
const char *SDL_GetGamepadStringForButton(SDL_GamepadButton button);
SDL_GamepadButton SDL_GetGamepadButtonFromString(const char *str);
const char *SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis);
SDL_GamepadAxis SDL_GetGamepadAxisFromString(const char *str);
void SDL_SetGamepadEventsEnabled(bool enabled);
bool SDL_GamepadEventsEnabled(void);
bool SDL_SetWindowTitle(SDL_Window *window, const char *title);
bool SDL_SetWindowRelativeMouseMode(SDL_Window *window, bool enabled);
bool SDL_GetWindowRelativeMouseMode(SDL_Window *window);
void __setAnimationFrameFunc(void (*callback)(void));

/* ---- Cursors (SDL_mouse.h; docs/archive/0105) ----
   System shapes only — the pointer is the native browser cursor (WM.md
   deviation: no kernel sprite), so a cursor is just a CSS-name enum the host
   feeds to the canvas cursor style. SDL_CreateCursor (custom pixel cursors)
   stays out of scope. The kernel derives chrome cursors (resize edges) from
   its own hit test and OVERLAYS them over the client cursor an app sets here;
   with one window per process, SDL_SetCursor targets that process's surface.
   SDL_SetCursor(NULL) is SDL3's "redraw the current cursor" no-op. */
typedef enum SDL_SystemCursor {
    SDL_SYSTEM_CURSOR_DEFAULT,        /* 0  arrow */
    SDL_SYSTEM_CURSOR_TEXT,           /* 1  I-beam */
    SDL_SYSTEM_CURSOR_WAIT,           /* 2 */
    SDL_SYSTEM_CURSOR_CROSSHAIR,      /* 3 */
    SDL_SYSTEM_CURSOR_PROGRESS,       /* 4 */
    SDL_SYSTEM_CURSOR_NWSE_RESIZE,    /* 5   diagonal */
    SDL_SYSTEM_CURSOR_NESW_RESIZE,    /* 6  / diagonal */
    SDL_SYSTEM_CURSOR_EW_RESIZE,      /* 7  horizontal */
    SDL_SYSTEM_CURSOR_NS_RESIZE,      /* 8  vertical */
    SDL_SYSTEM_CURSOR_MOVE,           /* 9  four-way */
    SDL_SYSTEM_CURSOR_NOT_ALLOWED,    /* 10 */
    SDL_SYSTEM_CURSOR_POINTER,        /* 11 hand */
    SDL_SYSTEM_CURSOR_NW_RESIZE,      /* 12 */
    SDL_SYSTEM_CURSOR_N_RESIZE,       /* 13 */
    SDL_SYSTEM_CURSOR_NE_RESIZE,      /* 14 */
    SDL_SYSTEM_CURSOR_E_RESIZE,       /* 15 */
    SDL_SYSTEM_CURSOR_SE_RESIZE,      /* 16 */
    SDL_SYSTEM_CURSOR_S_RESIZE,       /* 17 */
    SDL_SYSTEM_CURSOR_SW_RESIZE,      /* 18 */
    SDL_SYSTEM_CURSOR_W_RESIZE,       /* 19 */
    SDL_SYSTEM_CURSOR_COUNT
} SDL_SystemCursor;

typedef struct SDL_Cursor SDL_Cursor;
SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id);
bool SDL_SetCursor(SDL_Cursor *cursor);
SDL_Cursor *SDL_GetCursor(void);
SDL_Cursor *SDL_GetDefaultCursor(void);
void SDL_DestroyCursor(SDL_Cursor *cursor);
bool SDL_ShowCursor(void);
bool SDL_HideCursor(void);
bool SDL_CursorVisible(void);

/* ---- Clipboard (SDL_clipboard.h; docs/archive/0090) ----
   Text only. One system-wide slot held by the OS kernel — copy/paste crosses
   processes and survives the writer exiting; standalone runs get a
   process-local slot with identical semantics. Usable without SDL_Init (the
   win32 veneer's clipboard rides this from console-shaped processes).
   SDL_GetClipboardText follows SDL3's contract: never NULL, "" when empty or
   on error, caller frees with SDL_free. */
bool SDL_SetClipboardText(const char *text);
char *SDL_GetClipboardText(void);
bool SDL_HasClipboardText(void);
bool SDL_ClearClipboardData(void);
void SDL_free(void *mem);
/* ---- Allocation family (#601) ----
   Over the one libc heap (SDL pointers and libc pointers are interchangeable
   here by construction), with SDL3's zero-size contract: a zero request is
   bumped to 1 so the return is never NULL for lack of size. */
void *SDL_malloc(size_t size);
void *SDL_calloc(size_t nmemb, size_t size);
void *SDL_realloc(void *mem, size_t size);

/* ---- Pseudo-random numbers (#601, SDL_stdinc.h) ----
   Upstream SDL3's exact generator and scaling — NOT a libc rand alias:
   SDL_rand(n) is uniform over [0, n) via a 32x32 high multiply, and
   SDL_srand(0) seeds from the performance counter, as upstream. */
void SDL_srand(Uint64 seed);
Sint32 SDL_rand(Sint32 n);
Uint32 SDL_rand_bits(void);
float SDL_randf(void);

/* SDL_Log (#601): SDL3's INFO-priority app log — the formatted message to
   stderr with a trailing newline (upstream's default output function; INFO
   carries no prefix). */
void SDL_Log(const char *fmt, ...);

/* ---- Base / pref paths (#601, SDL_filesystem.h) ----
   SDL_GetBasePath: the directory of the running binary WITH the trailing
   slash, resolved the way the exec actually found it (argv[0] from the
   synthetic /proc, a bare name re-run through $PATH, symlinks chased) —
   SDL3 ownership: the pointer is cached and owned by SDL, do not free.
   Fails loud (NULL + error) where there is no /proc.
   SDL_GetPrefPath: a writable per-app dir under $HOME (the XDG data-home
   shape: $HOME/.local/share/<org>/<app>/), created if missing, WITH the
   trailing slash — the caller frees with SDL_free. org may be NULL/empty. */
const char *SDL_GetBasePath(void);
char *SDL_GetPrefPath(const char *org, const char *app);

SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid,
                                           const SDL_AudioSpec *spec,
                                           SDL_AudioStreamCallback callback,
                                           void *userdata);

#define SDL_AUDIO_BITSIZE(x) ((x)&0xff)
#define SDL_AUDIO_BYTESIZE(x) (SDL_AUDIO_BITSIZE(x)/8)
#define SDL_AUDIO_S32LE SDL_AUDIO_S32
#define SDL_AUDIO_F32LE SDL_AUDIO_F32
#define SDL_AUDIO_FRAMESIZE(x) (SDL_AUDIO_BYTESIZE((x).format)*(x).channels)
#define SDL_AUDIO_ISFLOAT(x) ((x)&0x0100)
#define SDL_AUDIO_ISSIGNED(x) ((x)&0x8000)
#define SDL_AUDIO_ISBIGENDIAN(x) ((x)&0x1000)
#define SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN "SDL.audiostream.auto_cleanup"
bool SDL_LockAudioStream(SDL_AudioStream *stream);
bool SDL_UnlockAudioStream(SDL_AudioStream *stream);
SDL_PropertiesID SDL_GetAudioStreamProperties(SDL_AudioStream *stream);
SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream *stream);
bool SDL_SetAudioStreamGetCallback(SDL_AudioStream *stream,SDL_AudioStreamCallback callback,void *userdata);
bool SDL_SetAudioStreamPutCallback(SDL_AudioStream *stream,SDL_AudioStreamCallback callback,void *userdata);
bool SDL_SetAudioStreamGain(SDL_AudioStream *stream,float gain);
float SDL_GetAudioStreamGain(SDL_AudioStream *stream);
bool SDL_SetAudioStreamFrequencyRatio(SDL_AudioStream *stream,float ratio);
float SDL_GetAudioStreamFrequencyRatio(SDL_AudioStream *stream);
bool SDL_SetAudioStreamInputChannelMap(SDL_AudioStream *stream,const int *map,int count);
bool SDL_SetAudioStreamOutputChannelMap(SDL_AudioStream *stream,const int *map,int count);
int *SDL_GetAudioStreamOutputChannelMap(SDL_AudioStream *stream,int *count);
int *SDL_GetAudioStreamInputChannelMap(SDL_AudioStream *stream,int *count);
bool SDL_AudioStreamDevicePaused(SDL_AudioStream *stream);

typedef void (SDLCALL *SDL_AudioStreamDataCompleteCallback)(void *userdata,const void *buf,int len);
bool SDL_PutAudioStreamDataNoCopy(SDL_AudioStream *stream,const void *buf,int len,SDL_AudioStreamDataCompleteCallback callback,void *userdata);
bool SDL_PutAudioStreamPlanarData(SDL_AudioStream *stream,const void * const *buffers,int channels,int samples);
bool SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len);
int SDL_GetAudioStreamQueued(SDL_AudioStream *stream);
bool SDL_ClearAudioStream(SDL_AudioStream *stream);
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream);
bool SDL_PauseAudioStreamDevice(SDL_AudioStream *stream);
void SDL_DestroyAudioStream(SDL_AudioStream *stream);

/* ---- Device-less audio streams + buffer mixing (#529-A) ----
   SDL_CreateAudioStream makes a MEMORY stream: a device-less format/rate/
   channel converter — put source bytes, get converted bytes back.
   Validation is SDL 3.4.0's: the five formats above, 1..8 channels (SDL's
   default channel-conversion matrix), any positive rate. Format changes are
   EPOCHED: queued bytes keep the specs they were put under; new specs apply
   to future puts (GetFormat reports the CURRENT configuration, so a reader
   crossing an epoch boundary sees the documented sequential format
   transition). SDL_GetAudioStreamData returns whole destination frames only
   (a buffer too short for one frame reads 0); SDL_GetAudioStreamAvailable
   reports exactly what GetData could return right now, without an implicit
   flush. SDL_FlushAudioStream marks a generation end (the finite resampler
   tail becomes readable) — not EOF: a later put starts a new epoch. On
   DEVICE streams (SDL_OpenAudioDeviceStream) GetData/Available are refused
   (the device consumes the converted side), a source-format change converts
   future puts to the device's open spec, a destination change is ignored as
   SDL specifies, and Pause/Resume remain device-only (MEMORY streams refuse
   them). SDL_MixAudio mixes src into dst with pinned SDL 3.4.0 semantics:
   volume quantized to 1/128 steps (0 = successful no-op, deliberately
   unclamped), truncating integer scaling, saturation to the integer range,
   float clamp to [-1, 1]; len must be a multiple of the sample width. */
SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec);
bool SDL_SetAudioStreamFormat(SDL_AudioStream *stream, const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec);
bool SDL_GetAudioStreamFormat(SDL_AudioStream *stream, SDL_AudioSpec *src_spec, SDL_AudioSpec *dst_spec);
int SDL_GetAudioStreamData(SDL_AudioStream *stream, void *buf, int len);
int SDL_GetAudioStreamAvailable(SDL_AudioStream *stream);
bool SDL_FlushAudioStream(SDL_AudioStream *stream);
bool SDL_MixAudio(Uint8 *dst, const Uint8 *src, SDL_AudioFormat format, Uint32 len, float volume);

/* ---- WAV file loading (#529-B / #723) ----
   The complete SDL3 SDL_LoadWAV, adapted VERBATIM from SDL release-3.4.0's
   src/audio/SDL_wave.c (zlib license; provenance + adaptation ledger:
   tests/unit/sdl_load_wav/upstream.json). Codecs: PCM 8/16/24/32 (24-bit
   returns S32), IEEE float32, MS ADPCM, IMA ADPCM, A-law, mu-law (ADPCM and
   the law codecs decode to S16). On success *audio_buf is an SDL-heap buffer
   — free it with SDL_free; a zero-sample file SUCCEEDS with *audio_buf NULL
   and *audio_len 0 (upstream behavior — check the length, not the pointer).
   On failure the spec/buffer/length outputs are zeroed, every partial
   allocation is freed, the file is closed, and SDL_GetError names the reason.
   The upstream SDL_HINT_WAVE_* lookups are FROZEN to their 3.4.0 defaults
   (RIFF size ignore-zero, truncation drop-block, fact chunk ignored, chunk
   limit 10000) — the hint store does not affect this loader. SDL_LoadWAV_IO
   stays ABSENT: there is no public SDL_IOStream (absence is honest,
   docs/PRINCIPLES.md). The decoder TU is DEMAND-LINKED below: a program
   that never references SDL_LoadWAV pays zero compile cost and zero bytes. */
bool SDL_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **audio_buf, Uint32 *audio_len);
__require_source_if("SDL_LoadWAV", "__SDL_wave.c");

/* ---- Error handling ----
   SDL keeps a per-thread error string; this runtime is single-threaded, so it's
   a single global. SDL_GetError never returns NULL (empty when no error).
   SDL_SetError always returns false and SDL_ClearError always returns true,
   matching SDL3's bool-returning convention (so `return SDL_SetError(...)` is
   the idiomatic early-out from a function that returns bool). */
const char *SDL_GetError(void);
bool SDL_SetError(const char *fmt, ...);
bool SDL_ClearError(void);
/* SDL3's standard "invalid parameter" helper (SDL_error.h). Expands to a
   SDL_SetError with SDL's exact wording, so SDL_GetError() after passing a NULL
   handle reads like upstream ("Parameter 'renderer' is invalid"). */
#define SDL_InvalidParamError(param) SDL_SetError("Parameter '%s' is invalid", (param))

/* ---- Hints (SDL_hints.h subset; ticket #551) ----
   Real SDL semantics for the store: SDL_SetHint/SDL_GetHint round-trip ANY
   name, and an environment variable of the hint's own name OVERRIDES a
   stored value (upstream's env-beats-normal-priority rule — which is also
   how `SDL_RENDER_DRIVER=software ./game` works with zero code changes).
   The one hint this runtime READS is SDL_HINT_RENDER_DRIVER, at
   SDL_CreateRenderer(name=NULL). A single driver name only — upstream's
   comma-separated priority list is not parsed (loud failure on a no-match). */
#define SDL_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"
bool SDL_SetHint(const char *name, const char *value);
const char *SDL_GetHint(const char *name);

/* ---- SDL_Renderer (2D accelerated) ---- */
SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, const char *name);
void SDL_DestroyRenderer(SDL_Renderer *renderer);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h);
SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface);
void SDL_DestroyTexture(SDL_Texture *texture);
bool SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
bool SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b);
bool SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha);
bool SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode);
bool SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode);
bool SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode scaleMode);
bool SDL_GetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode *scaleMode);
/* Clip state belongs to each render target; NULL disables, zero size clips all.
   Coordinates are target pixels; SDL_RenderClear ignores the clip. */
bool SDL_SetRenderClipRect(SDL_Renderer *renderer, const SDL_Rect *rect);
bool SDL_GetRenderClipRect(SDL_Renderer *renderer, SDL_Rect *rect);
bool SDL_RenderClipEnabled(SDL_Renderer *renderer);
bool SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
bool SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);
bool SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);
bool SDL_RenderClear(SDL_Renderer *renderer);
bool SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect);
bool SDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect, double angle, const SDL_FPoint *center, SDL_FlipMode flip);
bool SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect);
bool SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *rect);
bool SDL_RenderLine(SDL_Renderer *renderer, float x1, float y1, float x2, float y2);
bool SDL_RenderPoint(SDL_Renderer *renderer, float x, float y);
/* Batch draw calls (#601): loops over the singular primitives with
   upstream's argument contract. SDL_RenderLines draws count-1 CONNECTED
   segments (a polyline), not count/2 independent pairs. */
bool SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
bool SDL_RenderRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
bool SDL_RenderLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count);
bool SDL_RenderPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count);
bool SDL_RenderGeometry(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Vertex *vertices, int num_vertices, const int *indices, int num_indices);
/* Returns false (with SDL_GetError set) while a render target is bound —
   upstream documents that presenting to a texture target must fail. */
bool SDL_RenderPresent(SDL_Renderer *renderer);
/* Render targets (#496): 'texture' must be created with
   SDL_TEXTUREACCESS_TARGET; NULL restores the window as the target. Viewport/
   clip/scale do not exist in this runtime, so there is no per-target state for
   them. Drawing the currently-bound target as its own source is refused with
   an error (upstream leaves it undefined). */
bool SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
SDL_Texture *SDL_GetRenderTarget(SDL_Renderer *renderer);
/* Renderer vsync (#500): synchronize SDL_RenderPresent to the display clock —
   in gucOS, the compositor's real per-frame tick. vsync = N presents at most
   one frame per N display ticks (a paced present publishes its frame, then
   blocks to the tick — software tier; GPU-tier callback apps are paced at the
   frame driver). 0 disables pacing and is the DEFAULT on a fresh renderer,
   per upstream SDL3. Unsupported modes — adaptive (-1), or any pacing where
   no display clock exists (headless boot.js, standalone pages) — return
   false with SDL_GetError() set and leave the mode UNCHANGED.
   SDL_GetRenderVSync round-trips the value that was set; it does not report
   the platform's own frame cadence. Doc: /usr/share/doc/sdl-gucos.md. */
#define SDL_RENDERER_VSYNC_DISABLED 0
#define SDL_RENDERER_VSYNC_ADAPTIVE (-1)
bool SDL_SetRenderVSync(SDL_Renderer *renderer, int vsync);
bool SDL_GetRenderVSync(SDL_Renderer *renderer, int *vsync);
/* Debug text (#494): upstream SDL3's built-in 8x8 font (SDL 3.2+). Draws in
   the current render draw color at (x, y), 8 px per glyph — for debug
   overlays, scores, HUDs; use SDL_ttf for real typography. ASCII is the
   documented character set (upstream); input is UTF-8, unsupported
   codepoints draw upstream's "invalid" glyph. SDL_SetRenderScale does not
   exist in this runtime, so glyphs are always 8 window-pixels — the
   veneer-wide draw-in-window-pixels-1:1 rule. */
#define SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE 8
bool SDL_RenderDebugText(SDL_Renderer *renderer, float x, float y, const char *str);
bool SDL_RenderDebugTextFormat(SDL_Renderer *renderer, float x, float y, const char *fmt, ...);

 #include <__SDL_video.h>

/* ---- SDL3 main callbacks (SDL_init.h / SDL_main.h; ticket #551) ----
   SDL_AppResult is unconditional (SDL3 defines it in SDL_init.h); the
   callback entry machinery lives in <__SDL_main.h> and is emitted only when
   the app opted in with #define SDL_MAIN_USE_CALLBACKS before including
   this header (or before <SDL3/SDL_main.h>, which repeats this include —
   both orderings SDL3 documents compile). The callback model is THE
   sanctioned frame loop for GPU-presenting gucOS apps: main() returns, the
   host paces SDL_AppIterate at the compositor's cadence, and the process
   yields between frames — a blocking main loop that presents GPU frames is
   refused at its first present (host.js, #551). */
typedef enum SDL_AppResult {
    SDL_APP_CONTINUE,   /* keep iterating */
    SDL_APP_SUCCESS,    /* terminate with success (process exit 0) */
    SDL_APP_FAILURE     /* terminate with an error (process exit 1) */
} SDL_AppResult;

#include <__SDL_surface.h>
#include <__SDL_bmp.h>

#ifdef SDL_MAIN_USE_CALLBACKS
#include <__SDL_main.h>
#endif
  