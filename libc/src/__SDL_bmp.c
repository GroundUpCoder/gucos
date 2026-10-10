
#include <stdbool.h>
/* Bounded Windows BMP codec: uncompressed indexed8, RGB24 and RGB(A)32.
 * V3/V4/V5 canonical 32-bit bitfields preserve explicit alpha. RLE, OS/2,
 * arbitrary masks, embedded profiles and compressed codecs are unsupported.
 * Sequential I/O permits Python file-like streams without seeking. */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static Uint16 bmp_u16(const Uint8 *p){return (Uint16)(p[0]|((Uint16)p[1]<<8));}
static Uint32 bmp_u32(const Uint8 *p){return (Uint32)p[0]|((Uint32)p[1]<<8)|((Uint32)p[2]<<16)|((Uint32)p[3]<<24);}
static void bmp_put16(Uint8 *p,Uint16 n){p[0]=n;p[1]=n>>8;}
static void bmp_put32(Uint8 *p,Uint32 n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
static bool bmp_read(SDL_IOStream *io,void *buffer,size_t n){Uint8 *p=buffer;while(n){size_t got=SDL_ReadIO(io,p,n);if(!got||got>n)return SDL_SetError("Truncated BMP stream");p+=got;n-=got;}return true;}
static bool bmp_write(SDL_IOStream *io,const void *buffer,size_t n){const Uint8 *p=buffer;while(n){size_t got=SDL_WriteIO(io,p,n);if(!got||got>n)return SDL_SetError("BMP stream write failed");p+=got;n-=got;}return true;}
static bool bmp_skip(SDL_IOStream *io,Uint32 n){Uint8 scratch[512];while(n){size_t count=n>sizeof(scratch)?sizeof(scratch):n;if(!bmp_read(io,scratch,count))return false;n-=count;}return true;}

SDL_Surface *SDL_LoadBMP_IO(SDL_IOStream *io,bool closeio){
    SDL_Surface *surface=NULL;Uint8 *row=NULL;Uint8 file[14],dib[124]={0};
    Uint32 offset,declared,size,compression,colors,position,rowbytes,height;Sint32 width,signedheight;
    Uint16 bits;bool alpha=false,anyalpha=false,topdown=false,ok=false;
    if(!io){SDL_SetError("Invalid BMP input stream");return NULL;}
    if(!bmp_read(io,file,sizeof(file)))goto done;
    if(file[0]!='B'||file[1]!='M'){SDL_SetError("Not a Windows BMP file");goto done;}
    declared=bmp_u32(file+2);offset=bmp_u32(file+10);
    if(!bmp_read(io,dib,4))goto done;size=bmp_u32(dib);
    if(size!=40&&size!=52&&size!=56&&size!=108&&size!=124){SDL_SetError("Unsupported BMP DIB header");goto done;}
    if(!bmp_read(io,dib+4,size-4))goto done;
    width=(Sint32)bmp_u32(dib+4);signedheight=(Sint32)bmp_u32(dib+8);bits=bmp_u16(dib+14);compression=bmp_u32(dib+16);colors=bmp_u32(dib+32);
    if(width<=0||signedheight==0||signedheight==INT_MIN||bmp_u16(dib+12)!=1){SDL_SetError("Invalid BMP dimensions or planes");goto done;}
    topdown=signedheight<0;height=topdown?(Uint32)-signedheight:(Uint32)signedheight;
    if((bits!=8&&bits!=24&&bits!=32)||(compression!=0&&compression!=3)||(compression==3&&bits!=32)){
        SDL_SetError("Unsupported BMP pixel format or compression");goto done;
    }
    position=14+size;
    if(compression==3){
        Uint8 masks[16]={0};
        if(size==40){if(!bmp_read(io,masks,12))goto done;position+=12;}else memcpy(masks,dib+40,size>=56?16:12);
        Uint32 amask=bmp_u32(masks+12);
        if(bmp_u32(masks)!=0x00ff0000u||bmp_u32(masks+4)!=0x0000ff00u||bmp_u32(masks+8)!=0x000000ffu||(amask!=0&&amask!=0xff000000u)){
            SDL_SetError("Unsupported BMP channel masks");goto done;
        }
        alpha=amask!=0;
    }
    /* V5 linked/embedded color profiles need a color-management provider. */
    if(size==124&&(bmp_u32(dib+112)||bmp_u32(dib+116))){SDL_SetError("BMP color profiles are unsupported");goto done;}
    Uint64 rowwide=(((Uint64)(Uint32)width*bits+31)/32)*4;
    Uint64 pixelbytes=rowwide*height;
    if(rowwide>INT_MAX||pixelbytes>UINT_MAX||pixelbytes+offset>UINT_MAX||(declared&&pixelbytes+offset>declared)||
       (bmp_u32(dib+20)&&bmp_u32(dib+20)<pixelbytes)){
        SDL_SetError("Invalid or oversized BMP pixel extent");goto done;
    }
    rowbytes=(Uint32)rowwide;
    if(bits==8){if(!colors)colors=256;if(colors>256){SDL_SetError("Invalid BMP palette size");goto done;}position+=colors*4;}
    if(offset<position){SDL_SetError("BMP pixel offset overlaps header or palette");goto done;}
    surface=SDL_CreateSurface(width,(int)height,bits==8?SDL_PIXELFORMAT_INDEX8:bits==24?SDL_PIXELFORMAT_RGB24:SDL_PIXELFORMAT_RGBA32);
    if(!surface)goto done;
    if(bits==8){
        SDL_Palette *palette=SDL_CreateSurfacePalette(surface);if(!palette)goto done;
        SDL_Color entries[256];memset(entries,0,sizeof(entries));for(int i=0;i<256;i++)entries[i].a=255;
        for(Uint32 i=0;i<colors;i++){Uint8 entry[4];if(!bmp_read(io,entry,4))goto done;entries[i].r=entry[2];entries[i].g=entry[1];entries[i].b=entry[0];}
        if(!SDL_SetPaletteColors(palette,entries,0,256))goto done;
    }
    if(!bmp_skip(io,offset-position))goto done;
    row=malloc(rowbytes);if(!row){SDL_SetError("Out of memory loading BMP");goto done;}
    for(Uint32 y=0;y<height;y++){
        if(!bmp_read(io,row,rowbytes))goto done;
        int target=topdown?(int)y:(int)(height-y-1);
        if(bits==8){for(int x=0;x<width;x++)if(row[x]>=colors){SDL_SetError("BMP palette index out of range");goto done;}memcpy((Uint8*)surface->pixels+(size_t)target*surface->pitch,row,width);}
        else for(int x=0;x<width;x++){
            const Uint8 *p=row+(size_t)x*(bits/8);Uint8 a=bits==32&&(alpha||compression==0)?p[3]:255;
            if(a)anyalpha=true;
            if(!SDL_WriteSurfacePixel(surface,x,target,p[2],p[1],p[0],a))goto done;
        }
    }
    /* Legacy BI_RGB uses an unused high byte. Match SDL's all-zero alpha repair;
     * explicit BI_BITFIELDS alpha never takes this path (transparent images). */
    if(bits==32&&compression==0&&!anyalpha)for(int y=0;y<surface->h;y++)for(int x=0;x<surface->w;x++){
        Uint8 r,g,b,a;if(!SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a)||!SDL_WriteSurfacePixel(surface,x,y,r,g,b,255))goto done;
    }
    ok=true;
done:
    free(row);if(closeio&&!SDL_CloseIO(io))ok=false;
    if(!ok){SDL_DestroySurface(surface);surface=NULL;}return surface;
}

bool SDL_SaveBMP_IO(SDL_Surface *surface,SDL_IOStream *io,bool closeio){
    bool ok=false;Uint8 *row=NULL;Uint8 header[122]={0};
    if(!io){SDL_SetError("Invalid BMP output stream");return false;}
    if(!surface||!surface->pixels||surface->w<=0||surface->h<=0){SDL_SetError("Invalid BMP source surface");goto done;}
    const SDL_PixelFormatDetails *format=SDL_GetPixelFormatDetails(surface->format);if(!format)goto done;
    SDL_Palette *palette=SDL_GetSurfacePalette(surface);bool indexed=surface->format==SDL_PIXELFORMAT_INDEX8;
    if(indexed&&(!palette||palette->ncolors<=0||palette->ncolors>256)){SDL_SetError("Invalid BMP source palette");goto done;}
    Uint16 bits=indexed?8:format->Amask?32:24;Uint32 dibsize=bits==32?108:40;
    Uint32 count=indexed?(Uint32)palette->ncolors:0,offset=14+dibsize+count*4;
    Uint64 rowwide=(((Uint64)(Uint32)surface->w*bits+31)/32)*4,pixelbytes=rowwide*(Uint32)surface->h;
    if(rowwide>INT_MAX||pixelbytes+offset>UINT_MAX){SDL_SetError("BMP source dimensions exceed file limits");goto done;}
    Uint32 rowbytes=(Uint32)rowwide,total=(Uint32)pixelbytes+offset;
    header[0]='B';header[1]='M';bmp_put32(header+2,total);bmp_put32(header+10,offset);bmp_put32(header+14,dibsize);
    bmp_put32(header+18,surface->w);bmp_put32(header+22,surface->h);bmp_put16(header+26,1);bmp_put16(header+28,bits);
    bmp_put32(header+30,bits==32?3:0);bmp_put32(header+34,(Uint32)pixelbytes);bmp_put32(header+46,count);
    if(bits==32){bmp_put32(header+54,0x00ff0000u);bmp_put32(header+58,0x0000ff00u);bmp_put32(header+62,0x000000ffu);bmp_put32(header+66,0xff000000u);bmp_put32(header+70,0x73524742u);}
    if(!bmp_write(io,header,14+dibsize))goto done;
    for(Uint32 i=0;i<count;i++){Uint8 e[4]={palette->colors[i].b,palette->colors[i].g,palette->colors[i].r,0};if(!bmp_write(io,e,4))goto done;}
    row=calloc(1,rowbytes);if(!row){SDL_SetError("Out of memory saving BMP");goto done;}
    for(int y=surface->h-1;y>=0;y--){
        if(indexed){memcpy(row,(Uint8*)surface->pixels+(size_t)y*surface->pitch,surface->w);for(int x=0;x<surface->w;x++)if(row[x]>=count){SDL_SetError("BMP source palette index out of range");goto done;}}
        else for(int x=0;x<surface->w;x++){Uint8 r,g,b,a;if(!SDL_ReadSurfacePixel(surface,x,y,&r,&g,&b,&a))goto done;Uint8 *p=row+(size_t)x*(bits/8);p[0]=b;p[1]=g;p[2]=r;if(bits==32)p[3]=a;}
        if(!bmp_write(io,row,rowbytes))goto done;
    }
    if(!SDL_FlushIO(io))goto done;ok=true;
done:
    free(row);if(closeio&&!SDL_CloseIO(io))ok=false;return ok;
}
SDL_Surface *SDL_LoadBMP(const char *path){SDL_IOStream *io=SDL_IOFromFile(path,"rb");return io?SDL_LoadBMP_IO(io,true):NULL;}
bool SDL_SaveBMP(SDL_Surface *surface,const char *path){SDL_IOStream *io=SDL_IOFromFile(path,"wb");return io?SDL_SaveBMP_IO(surface,io,true):false;}
