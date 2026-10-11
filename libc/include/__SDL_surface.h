
#pragma once
/* Software surface subset: indexed8 and common packed 8/16/24/32-bit formats. */
__require_source("__SDL_surface.c");
#define SDL_ALPHA_OPAQUE 255
#define SDL_ALPHA_TRANSPARENT 0
#define SDL_SURFACE_PREALLOCATED 0x00000001u
#define SDL_SURFACE_LOCK_NEEDED 0x00000002u
#define SDL_SURFACE_LOCKED 0x00000004u
#define SDL_SURFACE_SIMD_ALIGNED 0x00000008u
#define SDL_MUSTLOCK(S) (((S)->flags & SDL_SURFACE_LOCK_NEEDED) != 0)
#define SDL_BITSPERPIXEL(X) (((X) >> 8) & 0xff)
#define SDL_BYTESPERPIXEL(X) ((X) & 0xff)
#define SDL_ISPIXELFORMAT_INDEXED(X) (SDL_PIXELTYPE(X) >= 1 && SDL_PIXELTYPE(X) <= 3)
#define SDL_BLENDMODE_MUL 8
#define SDL_BLENDMODE_BLEND_PREMULTIPLIED 16
#define SDL_BLENDMODE_ADD_PREMULTIPLIED 32
#define SDL_BLENDMODE_INVALID 0x7fffffff
#define SDL_SCALEMODE_INVALID -1
#define SDL_COLORSPACE_UNKNOWN 0
#define SDL_COLORSPACE_SRGB 0x120005a0u
#define SDL_COLORSPACE_RGB_DEFAULT SDL_COLORSPACE_SRGB
typedef Uint32 SDL_Colorspace;
typedef struct SDL_Palette { int ncolors; SDL_Color *colors; Uint32 version; int refcount; } SDL_Palette;
typedef struct SDL_PixelFormatDetails {
    SDL_PixelFormat format; Uint8 bits_per_pixel, bytes_per_pixel, padding[2];
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint8 Rbits, Gbits, Bbits, Abits, Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormatDetails;
const SDL_PixelFormatDetails *SDL_GetPixelFormatDetails(SDL_PixelFormat format);
const char *SDL_GetPixelFormatName(SDL_PixelFormat format);
bool SDL_GetMasksForPixelFormat(SDL_PixelFormat format,int *bpp,Uint32 *r,Uint32 *g,Uint32 *b,Uint32 *a);
SDL_PixelFormat SDL_GetPixelFormatForMasks(int bpp,Uint32 r,Uint32 g,Uint32 b,Uint32 a);
SDL_Palette *SDL_CreatePalette(int ncolors);
void SDL_DestroyPalette(SDL_Palette *palette);
bool SDL_SetPaletteColors(SDL_Palette *palette,const SDL_Color *colors,int first,int count);
Uint32 SDL_MapRGB(const SDL_PixelFormatDetails *format,const SDL_Palette *palette,Uint8 r,Uint8 g,Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormatDetails *format,const SDL_Palette *palette,Uint8 r,Uint8 g,Uint8 b,Uint8 a);
void SDL_GetRGB(Uint32 pixel,const SDL_PixelFormatDetails *format,const SDL_Palette *palette,Uint8 *r,Uint8 *g,Uint8 *b);
void SDL_GetRGBA(Uint32 pixel,const SDL_PixelFormatDetails *format,const SDL_Palette *palette,Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a);
/* Keep byte-channel conversion local to consumers. The exported functions
 * remain the fallback and retain their ABI (including address-taking). */
static inline __attribute__((always_inline)) Uint32 __SDL_MapRGBA_fast(const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 r,Uint8 g,Uint8 b,Uint8 a) {
    if(f&&!SDL_ISPIXELFORMAT_INDEXED(f->format)&&f->Rbits==8&&f->Gbits==8&&f->Bbits==8&&(f->Abits==0||f->Abits==8))
        return (((Uint32)r<<f->Rshift)&f->Rmask)|(((Uint32)g<<f->Gshift)&f->Gmask)|(((Uint32)b<<f->Bshift)&f->Bmask)|(((Uint32)(f->Abits?a:0)<<f->Ashift)&f->Amask);
    return SDL_MapRGBA(f,p,r,g,b,a);
}
static inline __attribute__((always_inline)) void __SDL_GetRGBA_fast(Uint32 px,const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a) {
    if(f&&!SDL_ISPIXELFORMAT_INDEXED(f->format)&&f->Rbits==8&&f->Gbits==8&&f->Bbits==8&&(f->Abits==0||f->Abits==8)) {
        Uint8 cr=(px&f->Rmask)>>f->Rshift,cg=(px&f->Gmask)>>f->Gshift,cb=(px&f->Bmask)>>f->Bshift;
        Uint8 ca=f->Amask?(f->Abits?(px&f->Amask)>>f->Ashift:0):255;
        if(r)*r=cr;if(g)*g=cg;if(b)*b=cb;if(a)*a=ca;
        return;
    }
    SDL_GetRGBA(px,f,p,r,g,b,a);
}
#define SDL_MapRGBA(...) __SDL_MapRGBA_fast(__VA_ARGS__)
#define SDL_GetRGBA(...) __SDL_GetRGBA_fast(__VA_ARGS__)
SDL_Surface *SDL_CreateSurface(int w,int h,SDL_PixelFormat format);
SDL_Surface *SDL_CreateSurfaceFrom(int w,int h,SDL_PixelFormat format,void *pixels,int pitch);
SDL_Surface *SDL_DuplicateSurface(SDL_Surface *surface);
SDL_Surface *SDL_ConvertSurface(SDL_Surface *surface,SDL_PixelFormat format);
SDL_Surface *SDL_ConvertSurfaceAndColorspace(SDL_Surface *surface,SDL_PixelFormat format,SDL_Palette *palette,SDL_Colorspace colorspace,Uint32 props);
bool SDL_SetSurfaceColorspace(SDL_Surface *surface,SDL_Colorspace colorspace);
SDL_Colorspace SDL_GetSurfaceColorspace(SDL_Surface *surface);
SDL_Palette *SDL_CreateSurfacePalette(SDL_Surface *surface);
SDL_Palette *SDL_GetSurfacePalette(SDL_Surface *surface);
SDL_PropertiesID SDL_GetSurfaceProperties(SDL_Surface *surface);
bool SDL_SetSurfacePalette(SDL_Surface *surface,SDL_Palette *palette);
bool SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);
bool SDL_SetSurfaceRLE(SDL_Surface *surface,bool enabled);
bool SDL_SurfaceHasRLE(SDL_Surface *surface);
bool SDL_SetSurfaceColorKey(SDL_Surface *surface,bool enabled,Uint32 key);
bool SDL_SurfaceHasColorKey(SDL_Surface *surface);
bool SDL_GetSurfaceColorKey(SDL_Surface *surface,Uint32 *key);
bool SDL_SetSurfaceColorMod(SDL_Surface *surface,Uint8 r,Uint8 g,Uint8 b);
bool SDL_GetSurfaceColorMod(SDL_Surface *surface,Uint8 *r,Uint8 *g,Uint8 *b);
bool SDL_SetSurfaceAlphaMod(SDL_Surface *surface,Uint8 a);
bool SDL_GetSurfaceAlphaMod(SDL_Surface *surface,Uint8 *a);
bool SDL_SetSurfaceBlendMode(SDL_Surface *surface,SDL_BlendMode mode);
bool SDL_GetSurfaceBlendMode(SDL_Surface *surface,SDL_BlendMode *mode);
bool SDL_SetSurfaceClipRect(SDL_Surface *surface,const SDL_Rect *rect);
bool SDL_GetSurfaceClipRect(SDL_Surface *surface,SDL_Rect *rect);
Uint32 SDL_MapSurfaceRGB(SDL_Surface *surface,Uint8 r,Uint8 g,Uint8 b);
Uint32 SDL_MapSurfaceRGBA(SDL_Surface *surface,Uint8 r,Uint8 g,Uint8 b,Uint8 a);
bool SDL_FillSurfaceRect(SDL_Surface *surface,const SDL_Rect *rect,Uint32 color);
bool SDL_FillSurfaceRects(SDL_Surface *surface,const SDL_Rect *rects,int count,Uint32 color);
bool SDL_BlitSurface(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect);
bool SDL_BlitSurfaceUnchecked(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect);
bool SDL_BlitSurfaceScaled(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect,SDL_ScaleMode scale);
bool SDL_StretchSurface(SDL_Surface *src,const SDL_Rect *srcrect,SDL_Surface *dst,const SDL_Rect *dstrect,SDL_ScaleMode scale);
bool SDL_ReadSurfacePixel(SDL_Surface *surface,int x,int y,Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a);
bool SDL_WriteSurfacePixel(SDL_Surface *surface,int x,int y,Uint8 r,Uint8 g,Uint8 b,Uint8 a);
void __sdl_surface_cleanup(SDL_Surface *surface);
void __sdl_surface_resized(SDL_Surface *surface);
#define SDL_PIXELFORMAT_UNKNOWN 0
#define SDL_PIXELFORMAT_INDEX1LSB 0x11100100u
#define SDL_PIXELFORMAT_INDEX1MSB 0x11200100u
#define SDL_PIXELFORMAT_INDEX2LSB 0x1c100200u
#define SDL_PIXELFORMAT_INDEX2MSB 0x1c200200u
#define SDL_PIXELFORMAT_INDEX4LSB 0x12100400u
#define SDL_PIXELFORMAT_INDEX4MSB 0x12200400u
#define SDL_PIXELFORMAT_INDEX8 0x13000801u
#define SDL_PIXELFORMAT_RGB332 0x14110801u
#define SDL_PIXELFORMAT_XRGB4444 0x15120c02u
#define SDL_PIXELFORMAT_XBGR4444 0x15520c02u
#define SDL_PIXELFORMAT_XRGB1555 0x15130f02u
#define SDL_PIXELFORMAT_XBGR1555 0x15530f02u
#define SDL_PIXELFORMAT_ARGB4444 0x15321002u
#define SDL_PIXELFORMAT_RGBA4444 0x15421002u
#define SDL_PIXELFORMAT_ABGR4444 0x15721002u
#define SDL_PIXELFORMAT_BGRA4444 0x15821002u
#define SDL_PIXELFORMAT_ARGB1555 0x15331002u
#define SDL_PIXELFORMAT_RGBA5551 0x15441002u
#define SDL_PIXELFORMAT_ABGR1555 0x15731002u
#define SDL_PIXELFORMAT_BGRA5551 0x15841002u
#define SDL_PIXELFORMAT_RGB565 0x15151002u
#define SDL_PIXELFORMAT_BGR565 0x15551002u
#define SDL_PIXELFORMAT_RGB24 0x17101803u
#define SDL_PIXELFORMAT_BGR24 0x17401803u
#define SDL_PIXELFORMAT_RGBX8888 0x16261804u
#define SDL_PIXELFORMAT_XBGR8888 0x16561804u
#define SDL_PIXELFORMAT_BGRX8888 0x16661804u
#define SDL_PIXELFORMAT_ARGB8888 0x16362004u
#define SDL_PIXELFORMAT_BGRA8888 0x16862004u
#define SDL_PIXELFORMAT_XRGB2101010 0x16172004u
#define SDL_PIXELFORMAT_XBGR2101010 0x16572004u
#define SDL_PIXELFORMAT_ARGB2101010 0x16372004u
#define SDL_PIXELFORMAT_ABGR2101010 0x16772004u
#define SDL_PIXELFORMAT_RGB48 0x18103006u
#define SDL_PIXELFORMAT_BGR48 0x18403006u
#define SDL_PIXELFORMAT_RGBA64 0x18204008u
#define SDL_PIXELFORMAT_ARGB64 0x18304008u
#define SDL_PIXELFORMAT_BGRA64 0x18504008u
#define SDL_PIXELFORMAT_ABGR64 0x18604008u
#define SDL_PIXELFORMAT_RGB48_FLOAT 0x1a103006u
#define SDL_PIXELFORMAT_BGR48_FLOAT 0x1a403006u
#define SDL_PIXELFORMAT_RGBA64_FLOAT 0x1a204008u
#define SDL_PIXELFORMAT_ARGB64_FLOAT 0x1a304008u
#define SDL_PIXELFORMAT_BGRA64_FLOAT 0x1a504008u
#define SDL_PIXELFORMAT_ABGR64_FLOAT 0x1a604008u
#define SDL_PIXELFORMAT_RGB96_FLOAT 0x1b10600cu
#define SDL_PIXELFORMAT_BGR96_FLOAT 0x1b40600cu
#define SDL_PIXELFORMAT_RGBA128_FLOAT 0x1b208010u
#define SDL_PIXELFORMAT_ARGB128_FLOAT 0x1b308010u
#define SDL_PIXELFORMAT_BGRA128_FLOAT 0x1b508010u
#define SDL_PIXELFORMAT_ABGR128_FLOAT 0x1b608010u
#define SDL_PIXELFORMAT_YV12 0x32315659u
#define SDL_PIXELFORMAT_IYUV 0x56555949u
#define SDL_PIXELFORMAT_YUY2 0x32595559u
#define SDL_PIXELFORMAT_UYVY 0x59565955u
#define SDL_PIXELFORMAT_YVYU 0x55595659u
#define SDL_PIXELFORMAT_NV12 0x3231564eu
#define SDL_PIXELFORMAT_NV21 0x3132564eu
#define SDL_PIXELFORMAT_P010 0x30313050u
#define SDL_PIXELFORMAT_EXTERNAL_OES 0x2053454fu
#define SDL_PIXELFORMAT_MJPG 0x47504a4du
#define SDL_PIXELFORMAT_BGRA32 SDL_PIXELFORMAT_ARGB8888
#define SDL_PIXELFORMAT_ARGB32 SDL_PIXELFORMAT_BGRA8888
#define SDL_PIXELFORMAT_ABGR32 SDL_PIXELFORMAT_RGBA8888
#define SDL_PIXELFORMAT_RGBX32 SDL_PIXELFORMAT_XBGR8888
#define SDL_PIXELFORMAT_BGRX32 SDL_PIXELFORMAT_XRGB8888
#define SDL_PIXELFORMAT_XRGB32 SDL_PIXELFORMAT_BGRX8888
#define SDL_PIXELFORMAT_XBGR32 SDL_PIXELFORMAT_RGBX8888

