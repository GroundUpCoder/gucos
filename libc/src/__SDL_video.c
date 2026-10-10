
/* Browser-window metadata and query adapters. Native-only controls fail with
   a diagnostic when no host operation exists; stored metadata never pretends
   to change the actual window system. */
#include <SDL.h>
#include <SDL_popup.h>
#include <__SDL_internal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
typedef struct WindowProps {struct WindowProps *next;SDL_Window *window;SDL_PropertiesID id;} WindowProps;
static WindowProps *window_props;
static bool video_live(SDL_Window *w){for(int i=0;i<__SDL_MAX_WINDOWS;i++)if(w&&__sdl_window_registry[i]==w)return 1;return SDL_SetError("Invalid window");}
SDL_PropertiesID SDL_GetWindowProperties(SDL_Window *w){if(!video_live(w))return 0;for(WindowProps *p=window_props;p;p=p->next)if(p->window==w)return p->id;WindowProps *p=calloc(1,sizeof(*p));if(!p){SDL_SetError("Out of memory");return 0;}p->id=SDL_CreateProperties();if(!p->id){free(p);return 0;}p->window=w;p->next=window_props;window_props=p;return p->id;}
void __sdl_forget_window(SDL_Window *w){WindowProps **p=&window_props;while(*p){WindowProps *v=*p;if(v->window==w){*p=v->next;SDL_DestroyProperties(v->id);free(v);return;}p=&v->next;}}
const char *SDL_GetWindowTitle(SDL_Window *w){if(!video_live(w))return "";return SDL_GetStringProperty(SDL_GetWindowProperties(w),"gucos.window.title","");}
SDL_Window *SDL_CreateWindowWithProperties(SDL_PropertiesID p){if(!p){SDL_SetError("Invalid window properties");return NULL;}const char *unsupported[]={"SDL.window.create.opengl","SDL.window.create.vulkan","SDL.window.create.metal","SDL.window.create.external_graphics_context","SDL.window.create.modal","SDL.window.create.mouse_grabbed"};for(size_t i=0;i<SDL_arraysize(unsupported);i++)if(SDL_GetBooleanProperty(p,unsupported[i],0)){SDL_SetError("Unsupported window property: %s",unsupported[i]);return NULL;}if(SDL_HasProperty(p,"SDL.window.create.win32.hwnd")||SDL_HasProperty(p,"SDL.window.create.x11.window")||SDL_HasProperty(p,"SDL.window.create.cocoa.window")){SDL_SetError("Native window embedding is unavailable");return NULL;}Sint64 width=SDL_GetNumberProperty(p,"SDL.window.create.width",640),height=SDL_GetNumberProperty(p,"SDL.window.create.height",480);if(width<1||height<1||width>32767||height>32767){SDL_SetError("Invalid window size");return NULL;}SDL_WindowFlags flags=SDL_GetNumberProperty(p,"SDL.window.create.flags",0);if(SDL_GetBooleanProperty(p,"SDL.window.create.fullscreen",0))flags|=SDL_WINDOW_FULLSCREEN;const char *names[]={"SDL.window.create.hidden","SDL.window.create.borderless","SDL.window.create.resizable","SDL.window.create.transparent","SDL.window.create.utility"};SDL_WindowFlags bits[]={SDL_WINDOW_HIDDEN,SDL_WINDOW_BORDERLESS,SDL_WINDOW_RESIZABLE,SDL_WINDOW_TRANSPARENT,SDL_WINDOW_UTILITY};for(int i=0;i<5;i++)if(SDL_HasProperty(p,names[i])){if(SDL_GetBooleanProperty(p,names[i],0))flags|=bits[i];else flags&=~bits[i];}const char *title=SDL_GetStringProperty(p,"SDL.window.create.title","");SDL_Window *w=SDL_CreateWindow(title,width,height,flags);if(!w)return NULL;SDL_Window *parent=SDL_GetPointerProperty(p,"SDL.window.create.parent",NULL);if(parent&&!SDL_SetWindowParent(w,parent)){SDL_DestroyWindow(w);return NULL;}return w;}
bool SDL_GetWindowPosition(SDL_Window *w,int *x,int *y){if(x)*x=0;if(y)*y=0;if(!video_live(w))return 0;return SDL_SetError("Window position is managed by the compositor and cannot be queried");}
bool SDL_SetWindowMinimumSize(SDL_Window *w,int width,int height){if(!video_live(w))return 0;if(width<0||height<0)return SDL_SetError("Invalid minimum window size");return SDL_SetError("Window minimum-size constraints are unavailable");}
bool SDL_SetWindowResizable(SDL_Window *w,bool enabled){if(!video_live(w))return 0;if(!!(w->flags&SDL_WINDOW_RESIZABLE)==!!enabled)return 1;return SDL_SetError("Changing window resize policy is unavailable");}
bool SDL_SetWindowBordered(SDL_Window *w,bool bordered){if(!video_live(w))return 0;if(!!(w->flags&SDL_WINDOW_BORDERLESS)==!bordered)return 1;return SDL_SetError("Changing window decorations is unavailable");}
bool SDL_MinimizeWindow(SDL_Window *w){if(!video_live(w))return 0;return SDL_SetError("Programmatic window minimization is unavailable");}
bool SDL_SetWindowFullscreen(SDL_Window *w,bool fullscreen){if(!video_live(w))return 0;if(!!(w->flags&SDL_WINDOW_FULLSCREEN)==!!fullscreen)return 1;return SDL_SetError("Changing fullscreen policy is unavailable on the VT platform");}
bool SDL_SetWindowFullscreenMode(SDL_Window *w,const SDL_DisplayMode *mode){if(!video_live(w))return 0;if(!mode)return 1;return SDL_SetError("Exclusive fullscreen display modes are unavailable");}
bool SDL_UpdateWindowSurfaceRects(SDL_Window *w,const SDL_Rect *rects,int count){if(count<0||(count&&!rects))return SDL_SetError("Invalid dirty rectangles");if(!video_live(w))return 0;return count?SDL_UpdateWindowSurface(w):1;}
bool SDL_GetWindowMouseGrab(SDL_Window *w){if(!video_live(w))return 0;return 0;}
bool SDL_SetWindowMouseGrab(SDL_Window *w,bool grabbed){if(!video_live(w))return 0;if(!grabbed)return 1;return SDL_SetError("Mouse confinement is unavailable; use relative mouse mode");}
SDL_DisplayID *SDL_GetDisplays(int *count){if(count)*count=0;SDL_Rect r;if(!SDL_GetDisplayBounds(1,&r))return NULL;SDL_DisplayID *ids=calloc(2,sizeof(*ids));if(!ids){SDL_SetError("Out of memory");return NULL;}ids[0]=1;if(count)*count=1;return ids;}
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window *w){return video_live(w)?1:0;}
bool SDL_GetDisplayUsableBounds(SDL_DisplayID id,SDL_Rect *rect){if(id!=1)return SDL_SetError("Invalid display");return SDL_GetDisplayBounds(id,rect);}
const SDL_DisplayMode *SDL_GetDesktopDisplayMode(SDL_DisplayID id){static SDL_DisplayMode mode;if(id!=1){SDL_SetError("Invalid display");return NULL;}SDL_Rect r;if(!SDL_GetDisplayBounds(id,&r))return NULL;SDL_zero(mode);mode.displayID=id;mode.format=SDL_PIXELFORMAT_RGBA32;mode.w=r.w;mode.h=r.h;mode.pixel_density=1.0f;mode.refresh_rate_denominator=1;return &mode;}
const SDL_DisplayMode *SDL_GetCurrentDisplayMode(SDL_DisplayID id){return SDL_GetDesktopDisplayMode(id);}
SDL_DisplayMode **SDL_GetFullscreenDisplayModes(SDL_DisplayID id,int *count){if(count)*count=0;if(!SDL_GetDesktopDisplayMode(id))return NULL;SDL_DisplayMode **list=calloc(1,sizeof(*list));if(!list)SDL_SetError("Out of memory");return list;}
bool SDL_GetClosestFullscreenDisplayMode(SDL_DisplayID id,int w,int h,float refresh,bool density,SDL_DisplayMode *mode){(void)id;(void)w;(void)h;(void)refresh;(void)density;(void)mode;return SDL_SetError("Exclusive fullscreen display modes are unavailable");}
int SDL_GetVersion(void){return SDL_VERSION;}
const char *SDL_GetCurrentVideoDriver(void){return SDL_WasInit(SDL_INIT_VIDEO)?"gucos":NULL;}
bool SDL_ScreenSaverEnabled(void){return 1;}
bool SDL_EnableScreenSaver(void){return 1;}
bool SDL_DisableScreenSaver(void){return SDL_SetError("Screensaver inhibition is unavailable");}
bool SDL_GetHintBoolean(const char *name,bool fallback){const char *s=SDL_GetHint(name);if(!s||!*s)return fallback;return *s!='0'&&strcasecmp(s,"false");}
SDL_Window *SDL_GetKeyboardFocus(void){for(int i=0;i<__SDL_MAX_WINDOWS;i++){SDL_Window *w=__sdl_window_registry[i];if(w&&(SDL_GetWindowFlags(w)&SDL_WINDOW_INPUT_FOCUS))return w;}return NULL;}

void SDL_WarpMouseInWindow(SDL_Window *w,float x,float y){(void)x;(void)y;if(video_live(w))SDL_SetError("Browser mouse warping is unavailable");}
bool SDL_StartTextInput(SDL_Window *w){if(!video_live(w))return 0;return SDL_SetBooleanProperty(SDL_GetWindowProperties(w),"gucos.text.active",1);}
bool SDL_StopTextInput(SDL_Window *w){if(!video_live(w))return 0;return SDL_SetBooleanProperty(SDL_GetWindowProperties(w),"gucos.text.active",0);}
bool SDL_TextInputActive(SDL_Window *w){if(!video_live(w))return 0;return SDL_GetBooleanProperty(SDL_GetWindowProperties(w),"gucos.text.active",0);}
bool SDL_SetTextInputArea(SDL_Window *w,const SDL_Rect *r,int cursor){(void)r;(void)cursor;if(!video_live(w))return 0;return SDL_SetError("IME text-area positioning is unavailable");}
SDL_Cursor *SDL_CreateCursor(const Uint8 *data,const Uint8 *mask,int w,int h,int hx,int hy){(void)data;(void)mask;(void)w;(void)h;(void)hx;(void)hy;SDL_SetError("Custom monochrome cursors are unavailable");return NULL;}
SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *s,int hx,int hy){(void)s;(void)hx;(void)hy;SDL_SetError("Custom color cursors are unavailable");return NULL;}
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* SDL 3.2.10 scancode name table, unmodified; browser key adapters below are new code. */
static const char *SDL_scancode_names[SDL_SCANCODE_COUNT] =
{
    /* 0 */ NULL,
    /* 1 */ NULL,
    /* 2 */ NULL,
    /* 3 */ NULL,
    /* 4 */ "A",
    /* 5 */ "B",
    /* 6 */ "C",
    /* 7 */ "D",
    /* 8 */ "E",
    /* 9 */ "F",
    /* 10 */ "G",
    /* 11 */ "H",
    /* 12 */ "I",
    /* 13 */ "J",
    /* 14 */ "K",
    /* 15 */ "L",
    /* 16 */ "M",
    /* 17 */ "N",
    /* 18 */ "O",
    /* 19 */ "P",
    /* 20 */ "Q",
    /* 21 */ "R",
    /* 22 */ "S",
    /* 23 */ "T",
    /* 24 */ "U",
    /* 25 */ "V",
    /* 26 */ "W",
    /* 27 */ "X",
    /* 28 */ "Y",
    /* 29 */ "Z",
    /* 30 */ "1",
    /* 31 */ "2",
    /* 32 */ "3",
    /* 33 */ "4",
    /* 34 */ "5",
    /* 35 */ "6",
    /* 36 */ "7",
    /* 37 */ "8",
    /* 38 */ "9",
    /* 39 */ "0",
    /* 40 */ "Return",
    /* 41 */ "Escape",
    /* 42 */ "Backspace",
    /* 43 */ "Tab",
    /* 44 */ "Space",
    /* 45 */ "-",
    /* 46 */ "=",
    /* 47 */ "[",
    /* 48 */ "]",
    /* 49 */ "\\",
    /* 50 */ "#",
    /* 51 */ ";",
    /* 52 */ "'",
    /* 53 */ "`",
    /* 54 */ ",",
    /* 55 */ ".",
    /* 56 */ "/",
    /* 57 */ "CapsLock",
    /* 58 */ "F1",
    /* 59 */ "F2",
    /* 60 */ "F3",
    /* 61 */ "F4",
    /* 62 */ "F5",
    /* 63 */ "F6",
    /* 64 */ "F7",
    /* 65 */ "F8",
    /* 66 */ "F9",
    /* 67 */ "F10",
    /* 68 */ "F11",
    /* 69 */ "F12",
    /* 70 */ "PrintScreen",
    /* 71 */ "ScrollLock",
    /* 72 */ "Pause",
    /* 73 */ "Insert",
    /* 74 */ "Home",
    /* 75 */ "PageUp",
    /* 76 */ "Delete",
    /* 77 */ "End",
    /* 78 */ "PageDown",
    /* 79 */ "Right",
    /* 80 */ "Left",
    /* 81 */ "Down",
    /* 82 */ "Up",
    /* 83 */ "Numlock",
    /* 84 */ "Keypad /",
    /* 85 */ "Keypad *",
    /* 86 */ "Keypad -",
    /* 87 */ "Keypad +",
    /* 88 */ "Keypad Enter",
    /* 89 */ "Keypad 1",
    /* 90 */ "Keypad 2",
    /* 91 */ "Keypad 3",
    /* 92 */ "Keypad 4",
    /* 93 */ "Keypad 5",
    /* 94 */ "Keypad 6",
    /* 95 */ "Keypad 7",
    /* 96 */ "Keypad 8",
    /* 97 */ "Keypad 9",
    /* 98 */ "Keypad 0",
    /* 99 */ "Keypad .",
    /* 100 */ "NonUSBackslash",
    /* 101 */ "Application",
    /* 102 */ "Power",
    /* 103 */ "Keypad =",
    /* 104 */ "F13",
    /* 105 */ "F14",
    /* 106 */ "F15",
    /* 107 */ "F16",
    /* 108 */ "F17",
    /* 109 */ "F18",
    /* 110 */ "F19",
    /* 111 */ "F20",
    /* 112 */ "F21",
    /* 113 */ "F22",
    /* 114 */ "F23",
    /* 115 */ "F24",
    /* 116 */ "Execute",
    /* 117 */ "Help",
    /* 118 */ "Menu",
    /* 119 */ "Select",
    /* 120 */ "Stop",
    /* 121 */ "Again",
    /* 122 */ "Undo",
    /* 123 */ "Cut",
    /* 124 */ "Copy",
    /* 125 */ "Paste",
    /* 126 */ "Find",
    /* 127 */ "Mute",
    /* 128 */ "VolumeUp",
    /* 129 */ "VolumeDown",
    /* 130 */ NULL,
    /* 131 */ NULL,
    /* 132 */ NULL,
    /* 133 */ "Keypad ,",
    /* 134 */ "Keypad = (AS400)",
    /* 135 */ "International 1",
    /* 136 */ "International 2",
    /* 137 */ "International 3",
    /* 138 */ "International 4",
    /* 139 */ "International 5",
    /* 140 */ "International 6",
    /* 141 */ "International 7",
    /* 142 */ "International 8",
    /* 143 */ "International 9",
    /* 144 */ "Language 1",
    /* 145 */ "Language 2",
    /* 146 */ "Language 3",
    /* 147 */ "Language 4",
    /* 148 */ "Language 5",
    /* 149 */ "Language 6",
    /* 150 */ "Language 7",
    /* 151 */ "Language 8",
    /* 152 */ "Language 9",
    /* 153 */ "AltErase",
    /* 154 */ "SysReq",
    /* 155 */ "Cancel",
    /* 156 */ "Clear",
    /* 157 */ "Prior",
    /* 158 */ "Return",
    /* 159 */ "Separator",
    /* 160 */ "Out",
    /* 161 */ "Oper",
    /* 162 */ "Clear / Again",
    /* 163 */ "CrSel",
    /* 164 */ "ExSel",
    /* 165 */ NULL,
    /* 166 */ NULL,
    /* 167 */ NULL,
    /* 168 */ NULL,
    /* 169 */ NULL,
    /* 170 */ NULL,
    /* 171 */ NULL,
    /* 172 */ NULL,
    /* 173 */ NULL,
    /* 174 */ NULL,
    /* 175 */ NULL,
    /* 176 */ "Keypad 00",
    /* 177 */ "Keypad 000",
    /* 178 */ "ThousandsSeparator",
    /* 179 */ "DecimalSeparator",
    /* 180 */ "CurrencyUnit",
    /* 181 */ "CurrencySubUnit",
    /* 182 */ "Keypad (",
    /* 183 */ "Keypad )",
    /* 184 */ "Keypad {",
    /* 185 */ "Keypad }",
    /* 186 */ "Keypad Tab",
    /* 187 */ "Keypad Backspace",
    /* 188 */ "Keypad A",
    /* 189 */ "Keypad B",
    /* 190 */ "Keypad C",
    /* 191 */ "Keypad D",
    /* 192 */ "Keypad E",
    /* 193 */ "Keypad F",
    /* 194 */ "Keypad XOR",
    /* 195 */ "Keypad ^",
    /* 196 */ "Keypad %",
    /* 197 */ "Keypad <",
    /* 198 */ "Keypad >",
    /* 199 */ "Keypad &",
    /* 200 */ "Keypad &&",
    /* 201 */ "Keypad |",
    /* 202 */ "Keypad ||",
    /* 203 */ "Keypad :",
    /* 204 */ "Keypad #",
    /* 205 */ "Keypad Space",
    /* 206 */ "Keypad @",
    /* 207 */ "Keypad !",
    /* 208 */ "Keypad MemStore",
    /* 209 */ "Keypad MemRecall",
    /* 210 */ "Keypad MemClear",
    /* 211 */ "Keypad MemAdd",
    /* 212 */ "Keypad MemSubtract",
    /* 213 */ "Keypad MemMultiply",
    /* 214 */ "Keypad MemDivide",
    /* 215 */ "Keypad +/-",
    /* 216 */ "Keypad Clear",
    /* 217 */ "Keypad ClearEntry",
    /* 218 */ "Keypad Binary",
    /* 219 */ "Keypad Octal",
    /* 220 */ "Keypad Decimal",
    /* 221 */ "Keypad Hexadecimal",
    /* 222 */ NULL,
    /* 223 */ NULL,
    /* 224 */ "Left Ctrl",
    /* 225 */ "Left Shift",
    /* 226 */ "Left Alt",
    /* 227 */ "Left GUI",
    /* 228 */ "Right Ctrl",
    /* 229 */ "Right Shift",
    /* 230 */ "Right Alt",
    /* 231 */ "Right GUI",
    /* 232 */ NULL,
    /* 233 */ NULL,
    /* 234 */ NULL,
    /* 235 */ NULL,
    /* 236 */ NULL,
    /* 237 */ NULL,
    /* 238 */ NULL,
    /* 239 */ NULL,
    /* 240 */ NULL,
    /* 241 */ NULL,
    /* 242 */ NULL,
    /* 243 */ NULL,
    /* 244 */ NULL,
    /* 245 */ NULL,
    /* 246 */ NULL,
    /* 247 */ NULL,
    /* 248 */ NULL,
    /* 249 */ NULL,
    /* 250 */ NULL,
    /* 251 */ NULL,
    /* 252 */ NULL,
    /* 253 */ NULL,
    /* 254 */ NULL,
    /* 255 */ NULL,
    /* 256 */ NULL,
    /* 257 */ "ModeSwitch",
    /* 258 */ "Sleep",
    /* 259 */ "Wake",
    /* 260 */ "ChannelUp",
    /* 261 */ "ChannelDown",
    /* 262 */ "MediaPlay",
    /* 263 */ "MediaPause",
    /* 264 */ "MediaRecord",
    /* 265 */ "MediaFastForward",
    /* 266 */ "MediaRewind",
    /* 267 */ "MediaTrackNext",
    /* 268 */ "MediaTrackPrevious",
    /* 269 */ "MediaStop",
    /* 270 */ "Eject",
    /* 271 */ "MediaPlayPause",
    /* 272 */ "MediaSelect",
    /* 273 */ "AC New",
    /* 274 */ "AC Open",
    /* 275 */ "AC Close",
    /* 276 */ "AC Exit",
    /* 277 */ "AC Save",
    /* 278 */ "AC Print",
    /* 279 */ "AC Properties",
    /* 280 */ "AC Search",
    /* 281 */ "AC Home",
    /* 282 */ "AC Back",
    /* 283 */ "AC Forward",
    /* 284 */ "AC Stop",
    /* 285 */ "AC Refresh",
    /* 286 */ "AC Bookmarks",
    /* 287 */ "SoftLeft",
    /* 288 */ "SoftRight",
    /* 289 */ "Call",
    /* 290 */ "EndCall",
};
static SDL_Keycode scan_key(SDL_Scancode sc){if(sc>=4&&sc<=29)return 'a'+sc-4;if(sc>=30&&sc<=38)return '1'+sc-30;if(sc==39)return '0';switch(sc){case 40:return 13;case 41:return 27;case 42:return 8;case 43:return 9;case 44:return 32;case 45:return '-';case 46:return '=';case 47:return '[';case 48:return ']';case 49:return '\\';case 51:return ';';case 52:return '\'';case 53:return '`';case 54:return ',';case 55:return '.';case 56:return '/';case 76:return 127;default:return 0x40000000u|sc;}}
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key,SDL_Keymod *mod){if(mod)*mod=0;if(key&0x40000000u){SDL_Scancode sc=key&~0x40000000u;return sc<SDL_SCANCODE_COUNT?sc:0;}if(key>='A'&&key<='Z'){if(mod)*mod=SDL_KMOD_SHIFT;key+='a'-'A';}for(SDL_Scancode sc=4;sc<SDL_SCANCODE_COUNT;sc++)if(scan_key(sc)==key)return sc;return 0;}
const char *SDL_GetKeyName(SDL_Keycode key){if(!key)return "";SDL_Scancode sc=SDL_GetScancodeFromKey(key,NULL);if(sc<SDL_SCANCODE_COUNT&&SDL_scancode_names[sc])return SDL_scancode_names[sc];typedef struct KeyName {struct KeyName *next;SDL_Keycode key;char name[8];} KeyName;static KeyName *names;for(KeyName *n=names;n;n=n->next)if(n->key==key)return n->name;KeyName *n=calloc(1,sizeof(*n));if(!n){SDL_SetError("Out of memory");return "";}n->key=key;*SDL_UCS4ToUTF8(key,n->name)=0;n->next=names;names=n;return n->name;}
SDL_Keycode SDL_GetKeyFromName(const char *name){if(!name||!*name)return 0;size_t len=strlen(name);const char *end=name;SDL_Keycode key=SDL_StepUTF8(&end,&len);if(!len){if(key>='A'&&key<='Z')key+='a'-'A';return key;}for(SDL_Scancode sc=0;sc<SDL_SCANCODE_COUNT;sc++)if(SDL_scancode_names[sc]&&!strcasecmp(name,SDL_scancode_names[sc]))return scan_key(sc);return 0;}
#undef SDL_atoi
#undef SDL_fabs
#undef SDL_sqrt
#undef SDL_sqrtf
int SDL_atoi(const char *s){return atoi(s);}
int SDL_setenv_unsafe(const char *name,const char *value,int overwrite){return setenv(name,value,overwrite);}
int SDL_strcasecmp(const char *a,const char *b){return strcasecmp(a,b);}
int SDL_strncasecmp(const char *a,const char *b,size_t n){return strncasecmp(a,b,n);}
unsigned long long SDL_strtoull(const char *s,char **end,int base){return strtoull(s,end,base);}
int SDL_sscanf(const char *s,const char *fmt,...){va_list args;va_start(args,fmt);int result=vsscanf(s,fmt,args);va_end(args);return result;}
bool SDL_OutOfMemory(void){return SDL_SetError("Out of memory");}
float SDL_atan2f(float y,float x){return atan2f(y,x);}
float SDL_cosf(float v){return cosf(v);}
double SDL_scalbn(double v,int n){return ldexp(v,n);}
float SDL_sinf(float v){return sinf(v);}
float SDL_sqrtf(float v){return sqrtf(v);}
double SDL_sqrt(double v){return sqrt(v);}
double SDL_fabs(double v){return fabs(v);}
