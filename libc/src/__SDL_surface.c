
#include <SDL.h>
#undef SDL_MapRGBA
#undef SDL_GetRGBA
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
/* Internal surface state lives in SDL_Surface.reserved, whose public ABI is
 * deliberately opaque. Legacy image/font/window surfaces start it at NULL. */
typedef struct SurfaceData {
    SDL_PropertiesID properties;
    SDL_Palette *palette;
    SDL_Rect clip;
    Uint8 r,g,b,a;
    SDL_BlendMode blend;
    Uint32 key;
    int has_key, locks;
    SDL_Colorspace colorspace;
} SurfaceData;
static SurfaceData *surface_data(SDL_Surface *s) {
    if (!s) { SDL_InvalidParamError("surface"); return NULL; }
    SurfaceData *d = s->reserved;
    if (!d) {
        d = calloc(1,sizeof(*d));
        if (!d) { SDL_SetError("Out of memory"); return NULL; }
        d->clip.w=s->w; d->clip.h=s->h;
        d->r=d->g=d->b=d->a=255;
        d->blend=SDL_ISPIXELFORMAT_ALPHA(s->format)?SDL_BLENDMODE_BLEND:SDL_BLENDMODE_NONE;
        d->colorspace=SDL_COLORSPACE_SRGB;
        s->reserved=d;
    }
    return d;
}
void __sdl_surface_cleanup(SDL_Surface *s) {
    if (s && s->reserved) { SurfaceData *d=s->reserved; SDL_DestroyProperties(d->properties); SDL_DestroyPalette(d->palette); free(d); s->reserved=NULL; }
}
void __sdl_surface_resized(SDL_Surface *s) {
    if(s && s->reserved) { SurfaceData *d=s->reserved; d->clip.x=d->clip.y=0;d->clip.w=s->w;d->clip.h=s->h; }
}
void SDL_DestroySurface(SDL_Surface *s) {
    if (!s || !(s->flags & IMG_SURFACE_OWNED)) return;
    if (s->refcount>1) { --s->refcount; return; }
    __sdl_surface_cleanup(s);
    if (!(s->flags & SDL_SURFACE_PREALLOCATED)) {
        if (s->flags & SDL_SURFACE_SIMD_ALIGNED) SDL_aligned_free(s->pixels);
        else free(s->pixels);
    }
    free(s);
}
/* Generated constant pixel descriptors follow. */
static const SDL_PixelFormatDetails formats[]={
{SDL_PIXELFORMAT_INDEX8,8,1,{0,0},0x0u,0x0u,0x0u,0x0u,0,0,0,0,0,0,0,0},
{SDL_PIXELFORMAT_RGB332,8,1,{0,0},0xe0u,0x1cu,0x3u,0x0u,3,3,2,0,5,2,0,0},
{SDL_PIXELFORMAT_XRGB4444,12,2,{0,0},0xf00u,0xf0u,0xfu,0x0u,4,4,4,0,8,4,0,0},
{SDL_PIXELFORMAT_XBGR4444,12,2,{0,0},0xfu,0xf0u,0xf00u,0x0u,4,4,4,0,0,4,8,0},
{SDL_PIXELFORMAT_XRGB1555,15,2,{0,0},0x7c00u,0x3e0u,0x1fu,0x0u,5,5,5,0,10,5,0,0},
{SDL_PIXELFORMAT_XBGR1555,15,2,{0,0},0x1fu,0x3e0u,0x7c00u,0x0u,5,5,5,0,0,5,10,0},
{SDL_PIXELFORMAT_ARGB4444,16,2,{0,0},0xf00u,0xf0u,0xfu,0xf000u,4,4,4,4,8,4,0,12},
{SDL_PIXELFORMAT_RGBA4444,16,2,{0,0},0xf000u,0xf00u,0xf0u,0xfu,4,4,4,4,12,8,4,0},
{SDL_PIXELFORMAT_ABGR4444,16,2,{0,0},0xfu,0xf0u,0xf00u,0xf000u,4,4,4,4,0,4,8,12},
{SDL_PIXELFORMAT_BGRA4444,16,2,{0,0},0xf0u,0xf00u,0xf000u,0xfu,4,4,4,4,4,8,12,0},
{SDL_PIXELFORMAT_ARGB1555,16,2,{0,0},0x7c00u,0x3e0u,0x1fu,0x8000u,5,5,5,1,10,5,0,15},
{SDL_PIXELFORMAT_RGBA5551,16,2,{0,0},0xf800u,0x7c0u,0x3eu,0x1u,5,5,5,1,11,6,1,0},
{SDL_PIXELFORMAT_ABGR1555,16,2,{0,0},0x1fu,0x3e0u,0x7c00u,0x8000u,5,5,5,1,0,5,10,15},
{SDL_PIXELFORMAT_BGRA5551,16,2,{0,0},0x3eu,0x7c0u,0xf800u,0x1u,5,5,5,1,1,6,11,0},
{SDL_PIXELFORMAT_RGB565,16,2,{0,0},0xf800u,0x7e0u,0x1fu,0x0u,5,6,5,0,11,5,0,0},
{SDL_PIXELFORMAT_BGR565,16,2,{0,0},0x1fu,0x7e0u,0xf800u,0x0u,5,6,5,0,0,5,11,0},
{SDL_PIXELFORMAT_RGB24,24,3,{0,0},0xffu,0xff00u,0xff0000u,0x0u,8,8,8,0,0,8,16,0},
{SDL_PIXELFORMAT_BGR24,24,3,{0,0},0xff0000u,0xff00u,0xffu,0x0u,8,8,8,0,16,8,0,0},
{SDL_PIXELFORMAT_XRGB8888,24,4,{0,0},0xff0000u,0xff00u,0xffu,0x0u,8,8,8,0,16,8,0,0},
{SDL_PIXELFORMAT_XBGR8888,24,4,{0,0},0xffu,0xff00u,0xff0000u,0x0u,8,8,8,0,0,8,16,0},
{SDL_PIXELFORMAT_RGBX8888,24,4,{0,0},0xff000000u,0xff0000u,0xff00u,0x0u,8,8,8,0,24,16,8,0},
{SDL_PIXELFORMAT_BGRX8888,24,4,{0,0},0xff00u,0xff0000u,0xff000000u,0x0u,8,8,8,0,8,16,24,0},
{SDL_PIXELFORMAT_ARGB8888,32,4,{0,0},0xff0000u,0xff00u,0xffu,0xff000000u,8,8,8,8,16,8,0,24},
{SDL_PIXELFORMAT_RGBA8888,32,4,{0,0},0xff000000u,0xff0000u,0xff00u,0xffu,8,8,8,8,24,16,8,0},
{SDL_PIXELFORMAT_ABGR8888,32,4,{0,0},0xffu,0xff00u,0xff0000u,0xff000000u,8,8,8,8,0,8,16,24},
{SDL_PIXELFORMAT_BGRA8888,32,4,{0,0},0xff00u,0xff0000u,0xff000000u,0xffu,8,8,8,8,8,16,24,0},
};
static const char *format_names[]={"SDL_PIXELFORMAT_INDEX8","SDL_PIXELFORMAT_RGB332","SDL_PIXELFORMAT_XRGB4444","SDL_PIXELFORMAT_XBGR4444","SDL_PIXELFORMAT_XRGB1555","SDL_PIXELFORMAT_XBGR1555","SDL_PIXELFORMAT_ARGB4444","SDL_PIXELFORMAT_RGBA4444","SDL_PIXELFORMAT_ABGR4444","SDL_PIXELFORMAT_BGRA4444","SDL_PIXELFORMAT_ARGB1555","SDL_PIXELFORMAT_RGBA5551","SDL_PIXELFORMAT_ABGR1555","SDL_PIXELFORMAT_BGRA5551","SDL_PIXELFORMAT_RGB565","SDL_PIXELFORMAT_BGR565","SDL_PIXELFORMAT_RGB24","SDL_PIXELFORMAT_BGR24","SDL_PIXELFORMAT_XRGB8888","SDL_PIXELFORMAT_XBGR8888","SDL_PIXELFORMAT_RGBX8888","SDL_PIXELFORMAT_BGRX8888","SDL_PIXELFORMAT_ARGB8888","SDL_PIXELFORMAT_RGBA8888","SDL_PIXELFORMAT_ABGR8888","SDL_PIXELFORMAT_BGRA8888"};
const SDL_PixelFormatDetails *SDL_GetPixelFormatDetails(SDL_PixelFormat f) {
    for(unsigned i=0;i<sizeof(formats)/sizeof(formats[0]);i++)if(formats[i].format==f)return &formats[i];
    SDL_SetError("Unsupported pixel format"); return NULL;
}
const char *SDL_GetPixelFormatName(SDL_PixelFormat f) {
    for(unsigned i=0;i<sizeof(formats)/sizeof(formats[0]);i++)if(formats[i].format==f)return format_names[i];
    return "SDL_PIXELFORMAT_UNKNOWN";
}
bool SDL_GetMasksForPixelFormat(SDL_PixelFormat f,int *bpp,Uint32 *r,Uint32 *g,Uint32 *b,Uint32 *a) {
    const SDL_PixelFormatDetails *d=SDL_GetPixelFormatDetails(f);if(!d)return false;
    if(bpp)*bpp=d->bits_per_pixel;if(r)*r=d->Rmask;if(g)*g=d->Gmask;if(b)*b=d->Bmask;if(a)*a=d->Amask;return true;
}
SDL_PixelFormat SDL_GetPixelFormatForMasks(int bits,Uint32 r,Uint32 g,Uint32 b,Uint32 a) {
    if(!r&&!g&&!b&&!a) { if(bits==8)return SDL_PIXELFORMAT_INDEX8;if(bits==24)return SDL_PIXELFORMAT_RGB24;if(bits==32)return SDL_PIXELFORMAT_XRGB8888; }
    for(unsigned i=0;i<sizeof(formats)/sizeof(formats[0]);i++) { const SDL_PixelFormatDetails *d=&formats[i];
        if((d->bits_per_pixel==bits || d->bytes_per_pixel*8==bits)&&d->Rmask==r&&d->Gmask==g&&d->Bmask==b&&d->Amask==a)return d->format;
    }
    SDL_SetError("Unsupported pixel masks");return SDL_PIXELFORMAT_UNKNOWN;
}
SDL_Palette *SDL_CreatePalette(int n) {
    if(n<=0 || (size_t)n>SIZE_MAX/sizeof(SDL_Color)) { SDL_InvalidParamError("ncolors");return NULL; }
    SDL_Palette *p=malloc(sizeof(*p));if(!p){SDL_SetError("Out of memory");return NULL;}
    p->colors=malloc((size_t)n*sizeof(SDL_Color));if(!p->colors){free(p);SDL_SetError("Out of memory");return NULL;}
    memset(p->colors,255,(size_t)n*sizeof(SDL_Color));p->ncolors=n;p->version=1;p->refcount=1;return p;
}
void SDL_DestroyPalette(SDL_Palette *p) { if(p && --p->refcount==0){free(p->colors);free(p);} }
bool SDL_SetPaletteColors(SDL_Palette *p,const SDL_Color *colors,int first,int count) {
    if(!p)return SDL_InvalidParamError("palette");if(!colors)return SDL_InvalidParamError("colors");
    if(first<0||count<0||first>p->ncolors||count>p->ncolors-first)return SDL_SetError("Palette range out of bounds");
    memcpy(p->colors+first,colors,(size_t)count*sizeof(*colors));if(++p->version==0)p->version=1;return true;
}
Uint32 SDL_MapRGBA(const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 r,Uint8 g,Uint8 b,Uint8 a) {
    if(!f){SDL_InvalidParamError("format");return 0;}
    if(SDL_ISPIXELFORMAT_INDEXED(f->format)) { unsigned best=UINT_MAX,index=0;if(!p||!p->ncolors){SDL_SetError("Palette is required");return 0;}
        for(int i=0;i<p->ncolors;i++){int dr=r-p->colors[i].r,dg=g-p->colors[i].g,db=b-p->colors[i].b,da=a-p->colors[i].a;unsigned dist=dr*dr+dg*dg+db*db+da*da;if(dist<best){best=dist;index=i;if(!dist)break;}}return index;
    }
    return (((Uint32)(r>>(8-f->Rbits))<<f->Rshift)&f->Rmask)|(((Uint32)(g>>(8-f->Gbits))<<f->Gshift)&f->Gmask)|(((Uint32)(b>>(8-f->Bbits))<<f->Bshift)&f->Bmask)|(((Uint32)(a>>(8-f->Abits))<<f->Ashift)&f->Amask);
}
Uint32 SDL_MapRGB(const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 r,Uint8 g,Uint8 b){return SDL_MapRGBA(f,p,r,g,b,255);}
static Uint8 expand(Uint32 v,int bits){if(bits==8)return (Uint8)v;if(!bits)return 0;unsigned max=(1u<<bits)-1;return (Uint8)(v*255/max);}
void SDL_GetRGBA(Uint32 px,const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a) {
    SDL_Color c={0,0,0,255};
    if(!f)SDL_InvalidParamError("format");
    else if(SDL_ISPIXELFORMAT_INDEXED(f->format)){if(p&&px<(Uint32)p->ncolors)c=p->colors[px];}
    else{c.r=expand((px&f->Rmask)>>f->Rshift,f->Rbits);c.g=expand((px&f->Gmask)>>f->Gshift,f->Gbits);c.b=expand((px&f->Bmask)>>f->Bshift,f->Bbits);c.a=f->Amask?expand((px&f->Amask)>>f->Ashift,f->Abits):255;}
    if(r)*r=c.r;if(g)*g=c.g;if(b)*b=c.b;if(a)*a=c.a;
}
void SDL_GetRGB(Uint32 px,const SDL_PixelFormatDetails *f,const SDL_Palette *p,Uint8 *r,Uint8 *g,Uint8 *b){SDL_GetRGBA(px,f,p,r,g,b,NULL);}
static SDL_Surface *new_surface(int w,int h,SDL_PixelFormat f,void *pixels,int pitch,int borrowed) {
    const SDL_PixelFormatDetails *fmt=SDL_GetPixelFormatDetails(f);if(!fmt)return NULL;
    if(w<0||h<0||w>(INT_MAX-3)/fmt->bytes_per_pixel){SDL_SetError("Invalid surface dimensions");return NULL;}
    int minpitch=w*fmt->bytes_per_pixel;
    if(borrowed){if(pitch<0 || (pixels&&pitch<minpitch)){SDL_InvalidParamError("pitch");return NULL;}}
    else pitch=(minpitch+3)&~3;
    if(h && (size_t)pitch>SIZE_MAX/(size_t)h){SDL_SetError("Surface size overflow");return NULL;}
    SDL_Surface *s=calloc(1,sizeof(*s));if(!s){SDL_SetError("Out of memory");return NULL;}
    s->flags=IMG_SURFACE_OWNED|(borrowed?SDL_SURFACE_PREALLOCATED:0);s->format=f;s->w=w;s->h=h;s->pitch=pitch;s->refcount=1;
    if(!borrowed&&h&&pitch){pixels=calloc((size_t)h,(size_t)pitch);if(!pixels){free(s);SDL_SetError("Out of memory");return NULL;}}
    s->pixels=pixels;
    if(!surface_data(s)){SDL_DestroySurface(s);return NULL;}return s;
}
SDL_Surface *SDL_CreateSurface(int w,int h,SDL_PixelFormat f){return new_surface(w,h,f,NULL,0,0);}
SDL_Surface *SDL_CreateSurfaceFrom(int w,int h,SDL_PixelFormat f,void *pixels,int pitch){return new_surface(w,h,f,pixels,pitch,1);}
/* Read-only like upstream SDL: a palette query must not allocate state. */
SDL_Palette *SDL_GetSurfacePalette(SDL_Surface *s){SurfaceData *d=s?s->reserved:NULL;return d?d->palette:NULL;}
SDL_PropertiesID SDL_GetSurfaceProperties(SDL_Surface *s){SurfaceData *d=surface_data(s);if(!d)return 0;if(!d->properties)d->properties=SDL_CreateProperties();return d->properties;}
bool SDL_SetSurfacePalette(SDL_Surface *s,SDL_Palette *p){SurfaceData *d=surface_data(s);if(!d)return false;
    if(!SDL_ISPIXELFORMAT_INDEXED(s->format))return SDL_SetError("Surface is not indexed");
    if(p&&p->ncolors>(1<<SDL_BITSPERPIXEL(s->format)))return SDL_SetError("Palette has too many colors");
    if(p)++p->refcount;SDL_DestroyPalette(d->palette);d->palette=p;return true;
}
SDL_Palette *SDL_CreateSurfacePalette(SDL_Surface *s){if(!s){SDL_InvalidParamError("surface");return NULL;}
    SDL_Palette *old=SDL_GetSurfacePalette(s);if(old)return old;
    if(!SDL_ISPIXELFORMAT_INDEXED(s->format)){SDL_SetError("Surface is not indexed");return NULL;}
    SDL_Palette *p=SDL_CreatePalette(1<<SDL_BITSPERPIXEL(s->format));if(!p)return NULL;
    if(!SDL_SetSurfacePalette(s,p)){SDL_DestroyPalette(p);return NULL;}SDL_DestroyPalette(p);return p;
}
bool SDL_LockSurface(SDL_Surface *s){SurfaceData *d=surface_data(s);if(!d)return false;if(d->locks==INT_MAX)return SDL_SetError("Too many surface locks");d->locks++;s->flags|=SDL_SURFACE_LOCKED;return true;}
void SDL_UnlockSurface(SDL_Surface *s){if(s&&s->reserved){SurfaceData *d=s->reserved;if(d->locks>0 && --d->locks==0)s->flags&=~SDL_SURFACE_LOCKED;}}
bool SDL_SetSurfaceRLE(SDL_Surface *s,bool enabled){(void)enabled;return surface_data(s)!=NULL;}
bool SDL_SurfaceHasRLE(SDL_Surface *s){if(!s)SDL_InvalidParamError("surface");return false;}
bool SDL_SetSurfaceColorKey(SDL_Surface *s,bool enabled,Uint32 key){SurfaceData *d=surface_data(s);if(!d)return false;d->has_key=enabled;d->key=key;return true;}
bool SDL_SurfaceHasColorKey(SDL_Surface *s){SurfaceData *d=surface_data(s);return d&&d->has_key;}
bool SDL_GetSurfaceColorKey(SDL_Surface *s,Uint32 *key){SurfaceData *d=surface_data(s);if(!d)return false;if(!key)return SDL_InvalidParamError("key");if(!d->has_key)return SDL_SetError("Surface has no color key");*key=d->key;return true;}
bool SDL_SetSurfaceColorMod(SDL_Surface *s,Uint8 r,Uint8 g,Uint8 b){SurfaceData*d=surface_data(s);if(!d)return false;d->r=r;d->g=g;d->b=b;return true;}
bool SDL_GetSurfaceColorMod(SDL_Surface *s,Uint8*r,Uint8*g,Uint8*b){SurfaceData*d=surface_data(s);if(!d)return false;if(r)*r=d->r;if(g)*g=d->g;if(b)*b=d->b;return true;}
bool SDL_SetSurfaceAlphaMod(SDL_Surface*s,Uint8 a){SurfaceData*d=surface_data(s);if(!d)return false;d->a=a;return true;}
bool SDL_GetSurfaceAlphaMod(SDL_Surface*s,Uint8*a){SurfaceData*d=surface_data(s);if(!d)return false;if(a)*a=d->a;return true;}
bool SDL_SetSurfaceBlendMode(SDL_Surface*s,SDL_BlendMode mode){SurfaceData*d=surface_data(s);if(!d)return false;
    if(mode!=SDL_BLENDMODE_NONE&&mode!=SDL_BLENDMODE_BLEND&&mode!=SDL_BLENDMODE_ADD&&mode!=SDL_BLENDMODE_MOD&&mode!=SDL_BLENDMODE_MUL&&mode!=SDL_BLENDMODE_BLEND_PREMULTIPLIED&&mode!=SDL_BLENDMODE_ADD_PREMULTIPLIED)return SDL_SetError("Unsupported surface blend mode");d->blend=mode;return true;
}
bool SDL_GetSurfaceBlendMode(SDL_Surface*s,SDL_BlendMode*mode){SurfaceData*d=surface_data(s);if(!d)return false;if(!mode)return SDL_InvalidParamError("blendMode");*mode=d->blend;return true;}
static bool intersect(SDL_Rect a,SDL_Rect b,SDL_Rect *out){Sint64 x=a.x>b.x?a.x:b.x,y=a.y>b.y?a.y:b.y;
    Sint64 ar=(Sint64)a.x+a.w,br=(Sint64)b.x+b.w,ab=(Sint64)a.y+a.h,bb=(Sint64)b.y+b.h;
    Sint64 right=ar<br?ar:br,bottom=ab<bb?ab:bb;
    out->x=(int)x;out->y=(int)y;out->w=right>x?(int)(right-x):0;out->h=bottom>y?(int)(bottom-y):0;return out->w>0&&out->h>0;
}
bool SDL_SetSurfaceClipRect(SDL_Surface*s,const SDL_Rect*r){SurfaceData*d=surface_data(s);if(!d)return false;SDL_Rect bounds={0,0,s->w,s->h};if(!r){d->clip=bounds;return true;}return intersect(bounds,*r,&d->clip);}
bool SDL_GetSurfaceClipRect(SDL_Surface*s,SDL_Rect*r){SurfaceData*d=surface_data(s);if(!d)return false;if(!r)return SDL_InvalidParamError("rect");*r=d->clip;return true;}
bool SDL_SetSurfaceColorspace(SDL_Surface*s,SDL_Colorspace c){SurfaceData*d=surface_data(s);if(!d)return false;if(c!=SDL_COLORSPACE_UNKNOWN&&c!=SDL_COLORSPACE_SRGB)return SDL_SetError("Unsupported surface colorspace");d->colorspace=c?c:SDL_COLORSPACE_SRGB;return true;}
SDL_Colorspace SDL_GetSurfaceColorspace(SDL_Surface*s){SurfaceData*d=surface_data(s);return d?d->colorspace:SDL_COLORSPACE_UNKNOWN;}
Uint32 SDL_MapSurfaceRGBA(SDL_Surface*s,Uint8 r,Uint8 g,Uint8 b,Uint8 a){if(!s){SDL_InvalidParamError("surface");return 0;}return SDL_MapRGBA(SDL_GetPixelFormatDetails(s->format),SDL_GetSurfacePalette(s),r,g,b,a);}
Uint32 SDL_MapSurfaceRGB(SDL_Surface*s,Uint8 r,Uint8 g,Uint8 b){return SDL_MapSurfaceRGBA(s,r,g,b,255);}
static Uint32 read_pixel(const SDL_Surface*s,int x,int y,int bytes){const Uint8*p=(const Uint8*)s->pixels+(size_t)y*s->pitch+(size_t)x*bytes;Uint32 n=0;for(int i=0;i<bytes;i++)n|=(Uint32)p[i]<<(i*8);return n;}
static void write_pixel(SDL_Surface*s,int x,int y,int bytes,Uint32 n){Uint8*p=(Uint8*)s->pixels+(size_t)y*s->pitch+(size_t)x*bytes;for(int i=0;i<bytes;i++)p[i]=(Uint8)(n>>(i*8));}
bool SDL_ReadSurfacePixel(SDL_Surface*s,int x,int y,Uint8*r,Uint8*g,Uint8*b,Uint8*a){SurfaceData*d=surface_data(s);if(!d)return false;if(x<0||y<0||x>=s->w||y>=s->h||!s->pixels)return SDL_SetError("Pixel outside surface");const SDL_PixelFormatDetails*f=SDL_GetPixelFormatDetails(s->format);if(!f)return false;SDL_GetRGBA(read_pixel(s,x,y,f->bytes_per_pixel),f,d->palette,r,g,b,a);return true;}
bool SDL_WriteSurfacePixel(SDL_Surface*s,int x,int y,Uint8 r,Uint8 g,Uint8 b,Uint8 a){SurfaceData*d=surface_data(s);if(!d)return false;if(x<0||y<0||x>=s->w||y>=s->h||!s->pixels)return SDL_SetError("Pixel outside surface");const SDL_PixelFormatDetails*f=SDL_GetPixelFormatDetails(s->format);if(!f)return false;write_pixel(s,x,y,f->bytes_per_pixel,SDL_MapRGBA(f,d->palette,r,g,b,a));return true;}
/* A packed pixel gives borrowed/odd-pitch surfaces an unaligned i32 store
   without imposing the alignment requirement of a Uint32 pointer cast. */
typedef struct __attribute__((packed)) { Uint32 value; } FillPixel32;
typedef struct __attribute__((packed)) { Uint64 value; } FillPixel64;
bool SDL_FillSurfaceRect(SDL_Surface*s,const SDL_Rect*r,Uint32 color){
    if(!s)return SDL_InvalidParamError("surface");
    const SDL_PixelFormatDetails*f=SDL_GetPixelFormatDetails(s->format);if(!f)return false;
    /* Filling only reads clip state. Avoid lazy allocation here: besides being
       unnecessary, a dynamic calloc call forces this whole loop to be framed. */
    SurfaceData*d=s->reserved;
    SDL_Rect area=d?d->clip:(SDL_Rect){0,0,s->w,s->h};
    if(r&&!intersect(area,*r,&area))return true;if(!area.w||!area.h)return true;
    if(!s->pixels)return SDL_SetError("Surface has no pixels");
    if(f->bytes_per_pixel==4){
        size_t rowbytes=(size_t)area.w*4;
        Uint8*first=(Uint8*)s->pixels+(size_t)area.y*s->pitch+(size_t)area.x*4;
        if(color==(color&255u)*0x01010101u){
            /* Emit bulk-memory instructions directly: dynamic libc memcpy /
               memset calls would reintroduce continuation compatibility.
               One memory.fill for a contiguous region; otherwise preserve the
               clip margins and row padding with a fill per row. */
            if(rowbytes==(size_t)s->pitch)__builtin(memory_fill,first,(int)(color&255u),rowbytes*area.h);
            else for(int y=0;y<area.h;y++)__builtin(memory_fill,first+(size_t)y*s->pitch,(int)(color&255u),rowbytes);
        }else{
            FillPixel32*row=(FillPixel32*)first;
            for(int x=0;x<area.w;x++)row[x].value=color;
            for(int y=1;y<area.h;y++)__builtin(memory_copy,first+(size_t)y*s->pitch,first,rowbytes);
        }
        return true;
    }
    for(int y=area.y;y<area.y+area.h;y++)for(int x=area.x;x<area.x+area.w;x++)write_pixel(s,x,y,f->bytes_per_pixel,color);
    return true;
}
bool SDL_FillSurfaceRects(SDL_Surface*s,const SDL_Rect*r,int n,Uint32 color){if(!s)return SDL_InvalidParamError("surface");if(n<0)return SDL_InvalidParamError("count");if(n&&!r)return SDL_InvalidParamError("rects");for(int i=0;i<n;i++)if(!SDL_FillSurfaceRect(s,r+i,color))return false;return true;}
static Uint8 sat(int x){return (Uint8)(x>255?255:x<0?0:x);}
static SDL_Color sample(SDL_Surface*s,const SDL_PixelFormatDetails*f,SurfaceData*d,int x,int y){SDL_Color c;SDL_GetRGBA(read_pixel(s,x,y,f->bytes_per_pixel),f,d->palette,&c.r,&c.g,&c.b,&c.a);return c;}
/* Common unscaled 32-bit surfaces have byte-aligned channels. Keep
 * their inner loop free of generic format conversion and scaling arithmetic.
 * Byte accesses also support unaligned borrowed surfaces and padded pitches. */
static unsigned div255(unsigned n){return (n+1+(n>>8))>>8;}
static void blit32(SDL_Surface *src,SDL_Surface *dst,SurfaceData *sd,
                   const SDL_PixelFormatDetails *sf,const SDL_PixelFormatDetails *df,
                   int sx,int sy,const SDL_Rect *area) {
    int r=sf->Rshift/8,g=sf->Gshift/8,b=sf->Bshift/8,aoff=6-r-g-b;
    int dr=df->Rshift/8,dg=df->Gshift/8,db=df->Bshift/8,dao=6-dr-dg-db;
    int mode=sd->blend,has_key=sd->has_key;Uint32 key=sd->key;
    unsigned alpha=sd->a;int sa=sf->Amask!=0,da=df->Amask!=0;
    if(mode==SDL_BLENDMODE_NONE&&alpha==255&&r==dr&&g==dg&&b==db){
        Uint32 mask=sf->Rmask|sf->Gmask|sf->Bmask|df->Amask,fill=sa?0:df->Amask;
        Uint64 mask2=((Uint64)mask<<32)|mask,fill2=((Uint64)fill<<32)|fill;
        for(int y=0;y<area->h;y++){
            const Uint8 *sp=(const Uint8*)src->pixels+(size_t)(sy+y)*src->pitch+(size_t)sx*4;
            Uint8 *dp=(Uint8*)dst->pixels+(size_t)(area->y+y)*dst->pitch+(size_t)area->x*4;
            if(!has_key&&sa&&da){memcpy(dp,sp,(size_t)area->w*4);continue;}
            /* Whole-pixel loads and stores; packed access keeps unaligned
               borrowed surfaces valid. Output bytes match the byte loop. */
            const FillPixel32 *s32=(const FillPixel32*)sp;FillPixel32 *d32=(FillPixel32*)dp;
            if(has_key){for(int x=0;x<area->w;x++){Uint32 p=s32[x].value;if(p!=key)d32[x].value=(p&mask)|fill;}}
            else{
                int x=0;
                for(;x+2<=area->w;x+=2)((FillPixel64*)(d32+x))->value=(((const FillPixel64*)(s32+x))->value&mask2)|fill2;
                for(;x<area->w;x++)d32[x].value=(s32[x].value&mask)|fill;
            }
        }
        return;
    }
    for(int y=0;y<area->h;y++) {
        const Uint8 *sp=(const Uint8*)src->pixels+(size_t)(sy+y)*src->pitch+(size_t)sx*4;
        Uint8 *dp=(Uint8*)dst->pixels+(size_t)(area->y+y)*dst->pitch+(size_t)area->x*4;
        for(int x=0;x<area->w;x++,sp+=4,dp+=4) {
            if(has_key){Uint32 p=(Uint32)sp[0]|((Uint32)sp[1]<<8)|((Uint32)sp[2]<<16)|((Uint32)sp[3]<<24);if(p==key)continue;}
            unsigned a=div255((sa?sp[aoff]:255)*alpha),inv=255-a;
            if(mode==SDL_BLENDMODE_NONE){dp[dr]=sp[r];dp[dg]=sp[g];dp[db]=sp[b];dp[dao]=da?a:0;}
            else if(mode==SDL_BLENDMODE_BLEND){
                dp[dr]=div255(sp[r]*a+dp[dr]*inv);dp[dg]=div255(sp[g]*a+dp[dg]*inv);dp[db]=div255(sp[b]*a+dp[db]*inv);
                dp[dao]=da?a+div255(dp[dao]*inv):0;
            }else{
                dp[dr]=sat(div255(sp[r]*a)+dp[dr]);dp[dg]=sat(div255(sp[g]*a)+dp[dg]);dp[db]=sat(div255(sp[b]*a)+dp[db]);
                if(!da)dp[dao]=0;
            }
        }
    }
}
/* Opaque RGB24/BGR24 copies into byte-aligned 32-bit formats. Read exactly
 * three bytes: borrowed surfaces may end at the final pixel without padding. */
static void blit24to32(SDL_Surface *src,SDL_Surface *dst,
                       const SDL_PixelFormatDetails *sf,const SDL_PixelFormatDetails *df,
                       int sx,int sy,const SDL_Rect *area) {
    int r=sf->Rshift/8,g=sf->Gshift/8,b=sf->Bshift/8;
    int dr=df->Rshift/8,dg=df->Gshift/8,db=df->Bshift/8,da=6-dr-dg-db;
    Uint8 alpha=df->Amask?255:0;
    for(int y=0;y<area->h;y++) {
        const Uint8 *sp=(const Uint8*)src->pixels+(size_t)(sy+y)*src->pitch+(size_t)sx*3;
        Uint8 *dp=(Uint8*)dst->pixels+(size_t)(area->y+y)*dst->pitch+(size_t)area->x*4;
        for(int x=0;x<area->w;x++,sp+=3,dp+=4) {
            dp[dr]=sp[r];dp[dg]=sp[g];dp[db]=sp[b];dp[da]=alpha;
        }
    }
}
static bool blit(SDL_Surface*src,const SDL_Rect*sr,SDL_Surface*dst,const SDL_Rect*dr,int scaled,SDL_ScaleMode mode,int raw) {
    SurfaceData *sd=surface_data(src),*dd=surface_data(dst);if(!sd||!dd)return false;
    if(!raw&&(sd->locks||dd->locks))return SDL_SetError("Surfaces must not be locked during blit");
    const SDL_PixelFormatDetails *sf=SDL_GetPixelFormatDetails(src->format),*df=SDL_GetPixelFormatDetails(dst->format);if(!sf||!df)return false;
    if((SDL_ISPIXELFORMAT_INDEXED(src->format)&&!sd->palette)||(SDL_ISPIXELFORMAT_INDEXED(dst->format)&&!dd->palette))return SDL_SetError("Indexed blit requires a palette");
    if(mode!=SDL_SCALEMODE_NEAREST&&mode!=SDL_SCALEMODE_LINEAR)return SDL_InvalidParamError("scaleMode");
    SDL_Rect source={0,0,src->w,src->h};if(sr)source=*sr;
    SDL_Rect target={0,0,scaled?dst->w:source.w,scaled?dst->h:source.h};if(dr){target.x=dr->x;target.y=dr->y;if(scaled){target.w=dr->w;target.h=dr->h;}}
    if(source.w<=0||source.h<=0||target.w<=0||target.h<=0)return true;
    SDL_Rect area;if(!intersect(target,dd->clip,&area))return true;
    if(!src->pixels||!dst->pixels)return SDL_SetError("Surface has no pixels");
    /* Any overlapping memory, including distinct wrappers for subsurfaces,
       must read a stable copy. Snapshot the source's full pitch to retain layout. */
    Uint8 *copy=NULL;SDL_Surface view=*src;
    uintptr_t sp=(uintptr_t)src->pixels,dp=(uintptr_t)dst->pixels;
    size_t sn=(size_t)src->pitch*src->h,dn=(size_t)dst->pitch*dst->h;
    if(sp<dp+dn&&dp<sp+sn){copy=malloc(sn?sn:1);if(!copy)return SDL_SetError("Out of memory");memcpy(copy,src->pixels,sn);view.pixels=copy;src=&view;}
    Sint64 fastx=(Sint64)source.x+area.x-target.x,fasty=(Sint64)source.y+area.y-target.y;
    if(!scaled&&!raw&&!sd->has_key&&sd->blend==SDL_BLENDMODE_NONE&&
       sd->r==255&&sd->g==255&&sd->b==255&&sd->a==255&&
       sf->bytes_per_pixel==3&&sf->Rbits==8&&sf->Gbits==8&&sf->Bbits==8&&
       df->bytes_per_pixel==4&&df->Rbits==8&&df->Gbits==8&&df->Bbits==8&&
       fastx>=0&&fasty>=0&&fastx+area.w<=src->w&&fasty+area.h<=src->h){
        blit24to32(src,dst,sf,df,(int)fastx,(int)fasty,&area);free(copy);return true;
    }
    if(!scaled&&!raw&&sd->r==255&&sd->g==255&&sd->b==255&&
       sf->bytes_per_pixel==4&&df->bytes_per_pixel==4&&sf->Rbits==8&&sf->Gbits==8&&sf->Bbits==8&&
       df->Rbits==8&&df->Gbits==8&&df->Bbits==8&&
       (sd->blend==SDL_BLENDMODE_NONE||sd->blend==SDL_BLENDMODE_BLEND||sd->blend==SDL_BLENDMODE_ADD)&&
       fastx>=0&&fasty>=0&&fastx+area.w<=src->w&&fasty+area.h<=src->h){
        blit32(src,dst,sd,sf,df,(int)fastx,(int)fasty,&area);free(copy);return true;
    }
    /* Nearest scaling between identical byte-aligned 32-bit formats copies
       whole pixels. The source column advances with exact integer stepping,
       so it selects the same pixels as the generic division with O(1) state.
       Same-size requests stay on the generic loop below. */
    if(scaled&&mode==SDL_SCALEMODE_NEAREST&&src->format==dst->format&&
       (target.w!=source.w||target.h!=source.h)&&
       sf->bytes_per_pixel==4&&sf->Rbits==8&&sf->Gbits==8&&sf->Bbits==8&&(sf->Abits==8||sf->Abits==0)&&
       (raw||(!sd->has_key&&sd->blend==SDL_BLENDMODE_NONE&&sd->r==255&&sd->g==255&&sd->b==255&&sd->a==255))){
        Uint32 keep=df->Rmask|df->Gmask|df->Bmask|df->Amask;
        Sint64 first=((Sint64)area.x-target.x)*source.w;
        Sint64 q0=(Sint64)source.x+first/target.w;int r0=(int)(first%target.w);
        int qstep=source.w/target.w,rstep=source.w%target.w;
        for(int y=area.y;y<area.y+area.h;y++){
            Sint64 sy=(Sint64)source.y+((Sint64)y-target.y)*source.h/target.h;
            if(sy<0||sy>=src->h)continue;
            const FillPixel32 *srow=(const FillPixel32*)((const Uint8*)src->pixels+(size_t)sy*src->pitch);
            FillPixel32 *drow=(FillPixel32*)((Uint8*)dst->pixels+(size_t)y*dst->pitch+(size_t)area.x*4);
            Sint64 q=q0;int r=r0;
            for(int x=0;x<area.w;x++){
                if(q>=0&&q<src->w)drow[x].value=srow[(int)q].value&keep;
                q+=qstep;r+=rstep;if(r>=target.w){r-=target.w;q++;}
            }
        }
        free(copy);return true;
    }
    for(int y=area.y;y<area.y+area.h;y++)for(int x=area.x;x<area.x+area.w;x++) {
        Sint64 sx=(Sint64)source.x+((Sint64)x-target.x)*source.w/target.w;
        Sint64 sy=(Sint64)source.y+((Sint64)y-target.y)*source.h/target.h;
        if(sx<0||sy<0||sx>=src->w||sy>=src->h)continue;
        Uint32 px=read_pixel(src,(int)sx,(int)sy,sf->bytes_per_pixel);
        if(!raw&&sd->has_key&&px==sd->key)continue;
        SDL_Color c;SDL_GetRGBA(px,sf,sd->palette,&c.r,&c.g,&c.b,&c.a);
        if(scaled&&mode==SDL_SCALEMODE_LINEAR){
            double fx=source.x+(((double)x-target.x+0.5)*source.w/target.w)-0.5;
            double fy=source.y+(((double)y-target.y+0.5)*source.h/target.h)-0.5;
            int x0=(int)fx,y0=(int)fy;if(fx<x0)--x0;if(fy<y0)--y0;
            double ax=fx-x0,ay=fy-y0;int x1=x0+1,y1=y0+1;
            if(x0<source.x)x0=source.x;if(y0<source.y)y0=source.y;
            if(x1>=(Sint64)source.x+source.w)x1=source.x+source.w-1;if(y1>=(Sint64)source.y+source.h)y1=source.y+source.h-1;
            if(x0<0)x0=0;if(y0<0)y0=0;if(x1<0)x1=0;if(y1<0)y1=0;
            if(x0>=src->w)x0=src->w-1;if(x1>=src->w)x1=src->w-1;if(y0>=src->h)y0=src->h-1;if(y1>=src->h)y1=src->h-1;
            SDL_Color c00=sample(src,sf,sd,x0,y0),c10=sample(src,sf,sd,x1,y0),c01=sample(src,sf,sd,x0,y1),c11=sample(src,sf,sd,x1,y1);
            c.r=(Uint8)((c00.r*(1-ax)+c10.r*ax)*(1-ay)+(c01.r*(1-ax)+c11.r*ax)*ay+0.5);
            c.g=(Uint8)((c00.g*(1-ax)+c10.g*ax)*(1-ay)+(c01.g*(1-ax)+c11.g*ax)*ay+0.5);
            c.b=(Uint8)((c00.b*(1-ax)+c10.b*ax)*(1-ay)+(c01.b*(1-ax)+c11.b*ax)*ay+0.5);
            c.a=(Uint8)((c00.a*(1-ax)+c10.a*ax)*(1-ay)+(c01.a*(1-ax)+c11.a*ax)*ay+0.5);
        }
        if(!raw){c.r=(Uint8)(c.r*sd->r/255);c.g=(Uint8)(c.g*sd->g/255);c.b=(Uint8)(c.b*sd->b/255);c.a=(Uint8)(c.a*sd->a/255);}
        SDL_BlendMode blend=raw?SDL_BLENDMODE_NONE:sd->blend;
        if(blend!=SDL_BLENDMODE_NONE){
            SDL_Color d=sample(dst,df,dd,x,y);int a=c.a,inv=255-a;
            if(blend==SDL_BLENDMODE_BLEND){c.r=(Uint8)((c.r*a+d.r*inv)/255);c.g=(Uint8)((c.g*a+d.g*inv)/255);c.b=(Uint8)((c.b*a+d.b*inv)/255);c.a=(Uint8)(a+d.a*inv/255);}
            else if(blend==SDL_BLENDMODE_BLEND_PREMULTIPLIED){c.r=sat(c.r+d.r*inv/255);c.g=sat(c.g+d.g*inv/255);c.b=sat(c.b+d.b*inv/255);c.a=sat(a+d.a*inv/255);}
            else if(blend==SDL_BLENDMODE_ADD){c.r=sat(c.r*a/255+d.r);c.g=sat(c.g*a/255+d.g);c.b=sat(c.b*a/255+d.b);c.a=d.a;}
            else if(blend==SDL_BLENDMODE_ADD_PREMULTIPLIED){c.r=sat(c.r+d.r);c.g=sat(c.g+d.g);c.b=sat(c.b+d.b);c.a=d.a;}
            else if(blend==SDL_BLENDMODE_MOD){c.r=(Uint8)(c.r*d.r/255);c.g=(Uint8)(c.g*d.g/255);c.b=(Uint8)(c.b*d.b/255);c.a=d.a;}
            else if(blend==SDL_BLENDMODE_MUL){c.r=sat((c.r*d.r+d.r*inv)/255);c.g=sat((c.g*d.g+d.g*inv)/255);c.b=sat((c.b*d.b+d.b*inv)/255);c.a=d.a;}
        }
        write_pixel(dst,x,y,df->bytes_per_pixel,SDL_MapRGBA(df,dd->palette,c.r,c.g,c.b,c.a));
    }
    free(copy);return true;
}
bool SDL_BlitSurface(SDL_Surface*s,const SDL_Rect*sr,SDL_Surface*d,const SDL_Rect*dr){return blit(s,sr,d,dr,0,SDL_SCALEMODE_NEAREST,0);}
bool SDL_BlitSurfaceUnchecked(SDL_Surface*s,const SDL_Rect*sr,SDL_Surface*d,const SDL_Rect*dr){return blit(s,sr,d,dr,0,SDL_SCALEMODE_NEAREST,0);}
bool SDL_BlitSurfaceScaled(SDL_Surface*s,const SDL_Rect*sr,SDL_Surface*d,const SDL_Rect*dr,SDL_ScaleMode mode){return blit(s,sr,d,dr,1,mode,0);}
bool SDL_StretchSurface(SDL_Surface*s,const SDL_Rect*sr,SDL_Surface*d,const SDL_Rect*dr,SDL_ScaleMode mode){return blit(s,sr,d,dr,1,mode,1);}
SDL_Surface *SDL_ConvertSurfaceAndColorspace(SDL_Surface*s,SDL_PixelFormat f,SDL_Palette*p,SDL_Colorspace colorspace,Uint32 props){
    SurfaceData *sd=surface_data(s);if(!sd)return NULL;
    if(props){SDL_SetError("Surface conversion properties are not supported");return NULL;}
    if(colorspace&&colorspace!=SDL_COLORSPACE_SRGB){SDL_SetError("Unsupported surface colorspace");return NULL;}
    const SDL_PixelFormatDetails *sf=SDL_GetPixelFormatDetails(s->format),*df=SDL_GetPixelFormatDetails(f);if(!sf||!df)return NULL;
    if(SDL_ISPIXELFORMAT_INDEXED(s->format)&&!sd->palette){SDL_SetError("Source palette is required");return NULL;}
    SDL_Surface *d=SDL_CreateSurface(s->w,s->h,f);if(!d)return NULL;SurfaceData *dd=surface_data(d);
    if(SDL_ISPIXELFORMAT_INDEXED(f)){
        if(p){if(!SDL_SetSurfacePalette(d,p)){SDL_DestroySurface(d);return NULL;}}
        else{p=SDL_CreateSurfacePalette(d);if(!p){SDL_DestroySurface(d);return NULL;}for(int i=0;i<256;i++){p->colors[i].r=expand((i>>5)&7,3);p->colors[i].g=expand((i>>2)&7,3);p->colors[i].b=expand(i&3,2);p->colors[i].a=255;}}
    }
    if(s->w&&s->h&&!s->pixels){SDL_DestroySurface(d);SDL_SetError("Surface has no pixels");return NULL;}
    /* Byte-aligned RGB sources converting to byte-aligned 32-bit formats use
       the opaque blit loops. A color key that must become transparent alpha
       keeps the generic per-pixel conversion. */
    int direct=s->w>0&&s->h>0&&!(sd->has_key&&df->Amask)&&
       df->bytes_per_pixel==4&&df->Rbits==8&&df->Gbits==8&&df->Bbits==8&&(df->Abits==8||df->Abits==0)&&
       sf->Rbits==8&&sf->Gbits==8&&sf->Bbits==8&&
       (sf->bytes_per_pixel==3||(sf->bytes_per_pixel==4&&(sf->Abits==8||sf->Abits==0)));
    if(direct){
        SDL_Rect all={0,0,s->w,s->h};
        if(sf->bytes_per_pixel==3)blit24to32(s,d,sf,df,0,0,&all);
        else{SurfaceData plain;memset(&plain,0,sizeof(plain));plain.blend=SDL_BLENDMODE_NONE;plain.a=255;blit32(s,d,&plain,sf,df,0,0,&all);}
    }
    else for(int y=0;y<s->h;y++)for(int x=0;x<s->w;x++){
        Uint32 px=read_pixel(s,x,y,sf->bytes_per_pixel);SDL_Color c;SDL_GetRGBA(px,sf,sd->palette,&c.r,&c.g,&c.b,&c.a);
        if(sd->has_key&&px==sd->key&&df->Amask)c.a=0;
        write_pixel(d,x,y,df->bytes_per_pixel,SDL_MapRGBA(df,dd->palette,c.r,c.g,c.b,c.a));
    }
    dd->r=sd->r;dd->g=sd->g;dd->b=sd->b;dd->a=sd->a;dd->blend=sd->blend;dd->clip=sd->clip;
    if(sd->has_key&&!df->Amask){SDL_Color c;SDL_GetRGBA(sd->key,sf,sd->palette,&c.r,&c.g,&c.b,&c.a);dd->has_key=1;dd->key=SDL_MapRGBA(df,dd->palette,c.r,c.g,c.b,c.a);}
    if(sd->has_key&&df->Amask)dd->blend=SDL_BLENDMODE_BLEND;
    return d;
}
SDL_Surface *SDL_ConvertSurface(SDL_Surface*s,SDL_PixelFormat f){return SDL_ConvertSurfaceAndColorspace(s,f,NULL,0,0);}
SDL_Surface *SDL_DuplicateSurface(SDL_Surface*s){if(!s){SDL_InvalidParamError("surface");return NULL;}return SDL_ConvertSurfaceAndColorspace(s,s->format,SDL_GetSurfacePalette(s),0,0);}

