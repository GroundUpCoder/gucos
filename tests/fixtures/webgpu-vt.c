#include <SDL3/SDL.h>
#include <sdl3webgpu.h>
#include <stdio.h>
#include <stdlib.h>
static SDL_Window *window;
static WGPUInstance instance;
static WGPUSurface surface;
static WGPUDevice device;
static WGPUQueue queue;
static WGPUTextureFormat format;
static const char *name;
static int frame;
static void tick(void) {
 SDL_Event e;while(SDL_PollEvent(&e)) {
  if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='q')exit(0);
  if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key=='x')abort();
 }
 if(!device)return;
 int w,h;SDL_GetWindowSize(window,&w,&h);
 WGPUSurfaceConfiguration config={.device=device,.format=format,.usage=WGPUTextureUsage_RenderAttachment,.width=w,.height=h,.alphaMode=WGPUCompositeAlphaMode_Opaque,.presentMode=WGPUPresentMode_Fifo};
 wgpuSurfaceConfigure(surface,&config);
 WGPUSurfaceTexture texture={0};wgpuSurfaceGetCurrentTexture(surface,&texture);
 if(!texture.texture){puts("GPU-TEXTURE-FAILED");exit(2);}
 WGPUTextureView view=wgpuTextureCreateView(texture.texture,0);
 WGPURenderPassColorAttachment color={.view=view,.depthSlice=WGPU_DEPTH_SLICE_UNDEFINED,.loadOp=WGPULoadOp_Clear,.storeOp=WGPUStoreOp_Store,.clearValue={.r=0.2,.g=0.4,.b=(frame%60)/60.,.a=1}};
 WGPURenderPassDescriptor desc={.colorAttachmentCount=1,.colorAttachments=&color};
 WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,0);
 WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(enc,&desc);wgpuRenderPassEncoderEnd(pass);
 WGPUCommandBuffer command=wgpuCommandEncoderFinish(enc,0);wgpuQueueSubmit(queue,1,&command);wgpuSurfacePresent(surface);
 wgpuCommandBufferRelease(command);wgpuCommandEncoderRelease(enc);wgpuRenderPassEncoderRelease(pass);wgpuTextureViewRelease(view);wgpuTextureRelease(texture.texture);
 if(frame++%10==0){printf("GPU-FRAME-%s-%d\n",name,frame);fflush(stdout);}
}
static void got_device(WGPURequestDeviceStatus status,WGPUDevice d,WGPUStringView message,void *a,void *b) {
 if(status!=WGPURequestDeviceStatus_Success)exit(3);device=d;queue=wgpuDeviceGetQueue(d);
 printf("GPU-DEVICE-%s\n",name);fflush(stdout);
}
static void got_adapter(WGPURequestAdapterStatus status,WGPUAdapter adapter,WGPUStringView message,void *a,void *b) {
 if(status!=WGPURequestAdapterStatus_Success)exit(4);
 format=wgpuSurfaceGetPreferredFormat(surface,adapter);
 WGPURequestDeviceCallbackInfo callback={.mode=WGPUCallbackMode_AllowSpontaneous,.callback=got_device};wgpuAdapterRequestDevice(adapter,0,callback);
}
int main(int argc,char **argv) {
 name=argv[1];SDL_Init(SDL_INIT_VIDEO);window=SDL_CreateWindow(name,320,200,SDL_WINDOW_RESIZABLE);
 if(!window)return 5;instance=wgpuCreateInstance(0);surface=SDL_GetWGPUSurface(instance,window);if(!surface)return 6;
 WGPURequestAdapterCallbackInfo callback={.mode=WGPUCallbackMode_AllowSpontaneous,.callback=got_adapter};
 wgpuInstanceRequestAdapter(instance,0,callback);wgpuSetMainLoopCallback(tick);return 0;
}
