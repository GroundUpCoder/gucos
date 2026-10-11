
#include <webgpu.h>
#include <stdio.h>
#include <stdlib.h>

/* Low-level host imports — primitives only (handles are i32 indices into the
   host handle table; pointers are i32; clear color is f64). host.js never reads
   C struct layouts: __webgpu.c flattens descriptors here. See docs/WEBGPU.md. */
__import int  __wgpu_create_instance(void);
__import int  __wgpu_instance_create_surface(int instance);
__import void __wgpu_instance_request_adapter(int instance, WGPURequestAdapterCallback cb, void *ud1, void *ud2);
__import void __wgpu_adapter_request_device(int adapter, WGPURequestDeviceCallback cb, void *ud1, void *ud2);
__import int  __wgpu_device_get_queue(int device);
__import int  __wgpu_surface_get_preferred_format(int surface);
__import void __wgpu_surface_configure(int surface, int device, int format, int usage, int width, int height, int alphaMode, int presentMode, const int *viewFormatsPacked, int viewFormatsLen);
__import int  __wgpu_surface_get_current_texture(int surface);
__import void __wgpu_surface_present(int surface);
__import int  __wgpu_texture_create_view(int texture, int format, int dimension, int baseMip, int mipCount, int baseLayer, int layerCount, int aspect);
__import int  __wgpu_device_create_shader_module_wgsl(int device, const char *code, int codeLen);
__import int  __wgpu_device_create_render_pipeline(int device, int vsModule, const char *vsEntry, int vsEntryLen, int fsModule, const char *fsEntry, int fsEntryLen, const int *targetsPacked, int targetsLen, int topology, int stripIndexFormat, int cullMode, int frontFace, const int *vbLayout, int vbLayoutLen, int layout, int depthEnabled, int depthFormat, int depthWriteEnabled, int depthCompare, int depthBias, double depthBiasSlopeScale, double depthBiasClamp, const int *stencilPacked, int sampleCount, int sampleMask, int alphaToCoverage, const int *vsConstInts, int vsConstIntsLen, const double *vsConstVals, const int *fsConstInts, int fsConstIntsLen, const double *fsConstVals);
__import int  __wgpu_device_create_buffer(int device, int size, int usage, int mappedAtCreation);
__import void __wgpu_queue_write_buffer(int queue, int buffer, int bufferOffset, const void *data, int size);
__import void __wgpu_render_pass_set_vertex_buffer(int pass, int slot, int buffer, int offset, int size);
__import void __wgpu_render_pass_set_index_buffer(int pass, int buffer, int format, int offset, int size);
__import void __wgpu_render_pass_draw_indexed(int pass, int indexCount, int instanceCount, int firstIndex, int baseVertex, int firstInstance);
__import int  __wgpu_device_create_bind_group_layout(int device, const int *packed, int packedLen);
__import int  __wgpu_device_create_pipeline_layout(int device, const int *bgls, int count);
__import int  __wgpu_device_create_bind_group(int device, int layout, const int *packed, int packedLen);
__import void __wgpu_render_pass_set_bind_group(int pass, int index, int group, const int *offsets, int offsetCount);
__import int  __wgpu_device_create_texture(int device, int width, int height, int depthOrArrayLayers, int format, int usage, int dimension, int mipLevelCount, int sampleCount);
__import int  __wgpu_device_create_sampler(int device, int addrU, int addrV, int addrW, int magFilter, int minFilter, int mipmapFilter, double lodMinClamp, double lodMaxClamp, int maxAnisotropy, int compare);
__import void __wgpu_queue_write_texture(int queue, int texture, int mipLevel, int originX, int originY, int originZ, int aspect, const void *data, int dataSize, int offset, int bytesPerRow, int rowsPerImage, int width, int height, int depthOrArrayLayers);
__import void __wgpu_cmd_copy_texture_to_buffer(int encoder, int srcTexture, int mipLevel, int ox, int oy, int oz, int dstBuffer, int offset, int bytesPerRow, int rowsPerImage, int width, int height, int depth);
__import void __wgpu_buffer_map_async(int buffer, int mode, int offset, int size, WGPUBufferMapCallback cb, void *ud1, void *ud2);
__import int  __wgpu_buffer_get_size(int buffer);
__import void __wgpu_buffer_get_mapped_range(int buffer, int offset, int size, void *dst);
__import void __wgpu_buffer_unmap(int buffer);
__import void __wgpu_cmd_copy_buffer_to_buffer(int encoder, int src, int srcOffset, int dst, int dstOffset, int size);
__import int  __wgpu_device_create_compute_pipeline(int device, int module, const char *entry, int entryLen, int layout, const int *constInts, int constIntsLen, const double *constVals);
__import int  __wgpu_command_encoder_begin_compute_pass(int encoder);
__import void __wgpu_compute_pass_set_pipeline(int pass, int pipeline);
__import void __wgpu_compute_pass_set_bind_group(int pass, int index, int group, const int *offsets, int offsetCount);
__import void __wgpu_compute_pass_dispatch(int pass, int x, int y, int z);
__import void __wgpu_compute_pass_end(int pass);
__import void __wgpu_device_push_error_scope(int device, int filter);
__import void __wgpu_device_pop_error_scope(int device, WGPUPopErrorScopeCallback cb, void *ud1, void *ud2);
__import int  __wgpu_device_create_command_encoder(int device);
__import int  __wgpu_command_encoder_begin_render_pass(int encoder, const int *colorPacked, int colorLen, const double *clearPacked, int depthView, int depthLoadOp, int depthStoreOp, double depthClearValue, int depthReadOnly, int stencilLoadOp, int stencilStoreOp, int stencilClearValue, int stencilReadOnly);
__import void __wgpu_render_pass_set_stencil_reference(int pass, int reference);
__import void __wgpu_render_pass_set_pipeline(int pass, int pipeline);
__import void __wgpu_render_pass_draw(int pass, int vc, int ic, int fv, int fi);
__import void __wgpu_render_pass_end(int pass);
__import int  __wgpu_command_encoder_finish(int encoder);
__import void __wgpu_queue_submit_one(int queue, int commandBuffer);
__import void __wgpu_release(int handle);
__import void __sdl_set_animation_frame_func(void (*callback)(void));

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor *descriptor) {
    (void)descriptor;
    return (WGPUInstance)__wgpu_create_instance();
}

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor *descriptor) {
    (void)descriptor;
    return (WGPUSurface)__wgpu_instance_create_surface((int)instance);
}

static unsigned long long __wgpu_future_seq = 0;

WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance,
        const WGPURequestAdapterOptions *options, WGPURequestAdapterCallbackInfo callbackInfo) {
    (void)options;
    __wgpu_instance_request_adapter((int)instance, callbackInfo.callback,
                                    callbackInfo.userdata1, callbackInfo.userdata2);
    WGPUFuture f; f.id = ++__wgpu_future_seq; return f;
}

WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter,
        const WGPUDeviceDescriptor *descriptor, WGPURequestDeviceCallbackInfo callbackInfo) {
    (void)descriptor;
    __wgpu_adapter_request_device((int)adapter, callbackInfo.callback,
                                  callbackInfo.userdata1, callbackInfo.userdata2);
    WGPUFuture f; f.id = ++__wgpu_future_seq; return f;
}

/* Trampolines invoked by host.js when the requestAdapter/requestDevice promise
   settles. They rebuild the by-value WGPUStringView in C (the host only passes
   primitives) and call the user's callback through its table index. */
void __wgpu_call_adapter_cb(WGPURequestAdapterCallback cb, int status,
        WGPUAdapter adapter, const char *msg, int msgLen, void *ud1, void *ud2) {
    WGPUStringView sv; sv.data = msg; sv.length = (size_t)(msgLen < 0 ? 0 : msgLen);
    if (cb) cb((WGPURequestAdapterStatus)status, adapter, sv, ud1, ud2);
}
__export __wgpu_call_adapter_cb = __wgpu_call_adapter_cb;

void __wgpu_call_device_cb(WGPURequestDeviceCallback cb, int status,
        WGPUDevice device, const char *msg, int msgLen, void *ud1, void *ud2) {
    WGPUStringView sv; sv.data = msg; sv.length = (size_t)(msgLen < 0 ? 0 : msgLen);
    if (cb) cb((WGPURequestDeviceStatus)status, device, sv, ud1, ud2);
}
__export __wgpu_call_device_cb = __wgpu_call_device_cb;

void __wgpu_call_buffer_map_cb(WGPUBufferMapCallback cb, int status, void *ud1, void *ud2) {
    WGPUStringView sv; sv.data = 0; sv.length = 0;
    if (cb) cb((WGPUMapAsyncStatus)status, sv, ud1, ud2);
}
__export __wgpu_call_buffer_map_cb = __wgpu_call_buffer_map_cb;

void __wgpu_call_pop_error_cb(WGPUPopErrorScopeCallback cb, int status, int type, void *ud1, void *ud2) {
    WGPUStringView sv; sv.data = 0; sv.length = 0;   /* message is logged host-side */
    if (cb) cb((WGPUPopErrorScopeStatus)status, (WGPUErrorType)type, sv, ud1, ud2);
}
__export __wgpu_call_pop_error_cb = __wgpu_call_pop_error_cb;

WGPUQueue wgpuDeviceGetQueue(WGPUDevice device) {
    return (WGPUQueue)__wgpu_device_get_queue((int)device);
}

WGPUTextureFormat wgpuSurfaceGetPreferredFormat(WGPUSurface surface, WGPUAdapter adapter) {
    (void)adapter;
    return (WGPUTextureFormat)__wgpu_surface_get_preferred_format((int)surface);
}

void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration *config) {
    /* viewFormats packed: [ count, fmt0, fmt1, ... ]. */
    static int vf[1 + 16];
    int vc = (int)config->viewFormatCount;
    if (vc > 16) {
        fprintf(stderr, "wgpuSurfaceConfigure: too many viewFormats (%d > 16); raise vf[]\n", vc);
        abort();
    }
    int vfn = 0;
    vf[vfn++] = vc;
    for (int i = 0; i < vc; i++) vf[vfn++] = (int)config->viewFormats[i];
    __wgpu_surface_configure((int)surface, (int)config->device, (int)config->format,
        (int)config->usage, (int)config->width, (int)config->height, (int)config->alphaMode,
        (int)config->presentMode, vf, vfn);
}

void wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture *surfaceTexture) {
    int t = __wgpu_surface_get_current_texture((int)surface);
    surfaceTexture->nextInChain = 0;
    surfaceTexture->texture = (WGPUTexture)t;
    surfaceTexture->status = t ? WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
                               : WGPUSurfaceGetCurrentTextureStatus_Error;
}

void wgpuSurfacePresent(WGPUSurface surface) {
    /* Web canvas: presentation is implicit (the browser presents the configured
       context after the frame) — the host import is a no-op there. Under the OS
       it is the real present: gpu transport hands the frame to the kernel as an
       ImageBitmap; the Dawn (headless) tier does the readback -> shm flip. */
    __wgpu_surface_present((int)surface);
}

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor *d) {
    /* NULL descriptor (or all-zero fields) => host applies WebGPU defaults. */
    if (!d)
        return (WGPUTextureView)__wgpu_texture_create_view((int)texture, 0, 0, 0, 0, 0, 0, 0);
    return (WGPUTextureView)__wgpu_texture_create_view((int)texture,
        (int)d->format, (int)d->dimension, (int)d->baseMipLevel, (int)d->mipLevelCount,
        (int)d->baseArrayLayer, (int)d->arrayLayerCount, (int)d->aspect);
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor *descriptor) {
    const WGPUChainedStruct *c = descriptor->nextInChain;
    while (c) {
        if (c->sType == WGPUSType_ShaderSourceWGSL) {
            const WGPUShaderSourceWGSL *w = (const WGPUShaderSourceWGSL *)c;
            return (WGPUShaderModule)__wgpu_device_create_shader_module_wgsl(
                (int)device, w->code.data, (int)w->code.length);
        }
        c = c->next;
    }
    return (WGPUShaderModule)0;
}

/* Pack WGPUConstantEntry[] (pipeline-overridable constants) into a parallel int
   array (keys) + double array (values), the host staying struct-ignorant. Int
   layout: [ count, per entry: keyPtr, keyLen ]; dp[i] = value. Returns the int
   array length. Each stage gets its own static buffers (a render pipeline may
   set both vertex and fragment constants). */
static int __wgpu_pack_constants(const WGPUConstantEntry *cs, int count, int *ip, double *dp, int cap) {
    if (1 + count * 2 > cap) {
        fprintf(stderr, "wgpu: pipeline constants exceed packed cap (%d entries max); raise the buffer\n", (cap - 1) / 2);
        abort();
    }
    int n = 0;
    ip[n++] = count;
    for (int i = 0; i < count; i++) {
        ip[n++] = (int)cs[i].key.data;
        ip[n++] = (int)cs[i].key.length;
        dp[i] = cs[i].value;
    }
    return n;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor *desc) {
    int fsModule = 0, fsEntryLen = 0;
    const char *fsEntry = 0;
    if (desc->fragment) {
        fsModule = (int)desc->fragment->module;
        fsEntry = desc->fragment->entryPoint.data;
        fsEntryLen = (int)desc->fragment->entryPoint.length;
    }

    /* Flatten the variable-length vertex.buffers[] (buffers -> attributes) into
       ONE packed int array; the host stays struct-ignorant and reads only ints.
       Layout: [ bufferCount,
                 per buffer: arrayStride, stepMode, attrCount,
                   per attr: format, byteOffset, shaderLocation ].
       This is the packed-array convention all array-bearing WebGPU descriptors
       use here. */
    static int vb[256];
    int n = 0, cap = (int)(sizeof(vb) / sizeof(vb[0]));
    int bc = (int)desc->vertex.bufferCount;
    vb[n++] = bc;
    for (int i = 0; i < bc; i++) {
        const WGPUVertexBufferLayout *L = &desc->vertex.buffers[i];
        int ac = (int)L->attributeCount;
        if (n + 3 + ac * 3 > cap) {
            fprintf(stderr, "wgpuDeviceCreateRenderPipeline: vertex layout exceeds packed cap (%d ints); raise vb[]\n", cap);
            abort();
        }
        vb[n++] = (int)L->arrayStride;
        vb[n++] = (int)L->stepMode;
        vb[n++] = ac;
        for (int j = 0; j < ac; j++) {
            vb[n++] = (int)L->attributes[j].format;
            vb[n++] = (int)L->attributes[j].offset;
            vb[n++] = (int)L->attributes[j].shaderLocation;
        }
    }

    /* Color targets (MRT) + per-target write masks + blend, packed into ONE int
       array (the host stays struct-ignorant; same packed-array convention as the
       vertex layout above). Layout: [ targetCount,
         per target: format, writeMask, blendEnabled,
           colorOp, colorSrc, colorDst, alphaOp, alphaSrc, alphaDst ].
       NULL blend => blendEnabled 0 (host leaves the target unblended/opaque). */
    static int ct[1 + 8 * 9];
    int ctn = 0, ctcap = (int)(sizeof(ct) / sizeof(ct[0]));
    int targetCount = (desc->fragment && desc->fragment->targets) ? (int)desc->fragment->targetCount : 0;
    ct[ctn++] = targetCount;
    for (int t = 0; t < targetCount; t++) {
        const WGPUColorTargetState *T = &desc->fragment->targets[t];
        if (ctn + 9 > ctcap) {
            fprintf(stderr, "wgpuDeviceCreateRenderPipeline: color targets exceed packed cap (%d); raise ct[]\n", ctcap);
            abort();
        }
        ct[ctn++] = (int)T->format;
        ct[ctn++] = (int)T->writeMask;
        const WGPUBlendState *bl = T->blend;
        if (bl) {
            ct[ctn++] = 1;
            ct[ctn++] = (int)bl->color.operation; ct[ctn++] = (int)bl->color.srcFactor; ct[ctn++] = (int)bl->color.dstFactor;
            ct[ctn++] = (int)bl->alpha.operation; ct[ctn++] = (int)bl->alpha.srcFactor; ct[ctn++] = (int)bl->alpha.dstFactor;
        } else {
            ct[ctn++] = 0; ct[ctn++] = 0; ct[ctn++] = 0; ct[ctn++] = 0; ct[ctn++] = 0; ct[ctn++] = 0; ct[ctn++] = 0;
        }
    }

    /* Depth/stencil state. NULL => no depth testing (host skips). */
    int depthEnabled = 0, depthFormat = 0, depthWriteEnabled = 0, depthCompare = 0, depthBias = 0;
    double depthBiasSlopeScale = 0, depthBiasClamp = 0;
    if (desc->depthStencil) {
        depthEnabled = 1;
        depthFormat = (int)desc->depthStencil->format;
        depthWriteEnabled = (int)desc->depthStencil->depthWriteEnabled;
        depthCompare = (int)desc->depthStencil->depthCompare;
        depthBias = (int)desc->depthStencil->depthBias;
        depthBiasSlopeScale = (double)desc->depthStencil->depthBiasSlopeScale;
        depthBiasClamp = (double)desc->depthStencil->depthBiasClamp;
    }

    /* Stencil face states, packed (NULL => no stencil): [ frontCompare, frontFail,
       frontDepthFail, frontPass, backCompare, backFail, backDepthFail, backPass,
       readMask, writeMask ]. Configured when either face has a compare set. */
    static int stencil[10];
    const int *stencilPtr = 0;
    if (desc->depthStencil &&
        (desc->depthStencil->stencilFront.compare != WGPUCompareFunction_Undefined ||
         desc->depthStencil->stencilBack.compare != WGPUCompareFunction_Undefined)) {
        const WGPUStencilFaceState *f = &desc->depthStencil->stencilFront;
        const WGPUStencilFaceState *b = &desc->depthStencil->stencilBack;
        stencil[0] = (int)f->compare; stencil[1] = (int)f->failOp; stencil[2] = (int)f->depthFailOp; stencil[3] = (int)f->passOp;
        stencil[4] = (int)b->compare; stencil[5] = (int)b->failOp; stencil[6] = (int)b->depthFailOp; stencil[7] = (int)b->passOp;
        stencil[8] = (int)desc->depthStencil->stencilReadMask;
        stencil[9] = (int)desc->depthStencil->stencilWriteMask;
        stencilPtr = stencil;
    }

    /* Multisample state. count defaults to 1; mask defaults to all-ones. */
    int sampleCount = (int)(desc->multisample.count ? desc->multisample.count : 1);
    int sampleMask = (int)desc->multisample.mask;
    int alphaToCoverage = (int)desc->multisample.alphaToCoverageEnabled;

    /* Pipeline-overridable constants for the vertex + fragment stages. */
    static int vsc[1 + 64 * 2]; static double vscv[64];
    static int fsc[1 + 64 * 2]; static double fscv[64];
    int vscn = __wgpu_pack_constants(desc->vertex.constants, (int)desc->vertex.constantCount, vsc, vscv, (int)(sizeof(vsc) / sizeof(vsc[0])));
    int fscn = 1; fsc[0] = 0;
    if (desc->fragment)
        fscn = __wgpu_pack_constants(desc->fragment->constants, (int)desc->fragment->constantCount, fsc, fscv, (int)(sizeof(fsc) / sizeof(fsc[0])));

    return (WGPURenderPipeline)__wgpu_device_create_render_pipeline(
        (int)device,
        (int)desc->vertex.module, desc->vertex.entryPoint.data, (int)desc->vertex.entryPoint.length,
        fsModule, fsEntry, fsEntryLen,
        ct, ctn, (int)desc->primitive.topology, (int)desc->primitive.stripIndexFormat,
        (int)desc->primitive.cullMode, (int)desc->primitive.frontFace,
        vb, n, (int)desc->layout,
        depthEnabled, depthFormat, depthWriteEnabled, depthCompare, depthBias, depthBiasSlopeScale, depthBiasClamp, stencilPtr,
        sampleCount, sampleMask, alphaToCoverage,
        vsc, vscn, vscv, fsc, fscn, fscv);
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor *descriptor) {
    (void)descriptor;
    return (WGPUCommandEncoder)__wgpu_device_create_command_encoder((int)device);
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor *desc) {
    /* Color attachments, packed (host stays struct-ignorant). Ints:
       [ count, per attachment: view, resolveTarget, loadOp, storeOp, depthSlice ].
       clearValue (4 doubles/attachment) rides a parallel double array, in order. */
    static int ca[1 + 8 * 5];
    static double cc[8 * 4];
    int can = 0, cacap = (int)(sizeof(ca) / sizeof(ca[0])), ccn = 0;
    int colorCount = (desc->colorAttachments) ? (int)desc->colorAttachmentCount : 0;
    ca[can++] = colorCount;
    for (int i = 0; i < colorCount; i++) {
        const WGPURenderPassColorAttachment *att = &desc->colorAttachments[i];
        if (can + 5 > cacap) {
            fprintf(stderr, "wgpuCommandEncoderBeginRenderPass: color attachments exceed packed cap (%d); raise ca[]\n", cacap);
            abort();
        }
        ca[can++] = (int)att->view;
        ca[can++] = (int)att->resolveTarget;
        ca[can++] = (int)att->loadOp;
        ca[can++] = (int)att->storeOp;
        ca[can++] = (int)att->depthSlice;
        cc[ccn++] = att->clearValue.r; cc[ccn++] = att->clearValue.g;
        cc[ccn++] = att->clearValue.b; cc[ccn++] = att->clearValue.a;
    }
    int depthView = 0, depthLoadOp = 0, depthStoreOp = 0, depthReadOnly = 0;
    double depthClearValue = 1.0;
    int stencilLoadOp = 0, stencilStoreOp = 0, stencilClearValue = 0, stencilReadOnly = 0;
    if (desc->depthStencilAttachment) {
        const WGPURenderPassDepthStencilAttachment *d = desc->depthStencilAttachment;
        depthView = (int)d->view;
        depthLoadOp = (int)d->depthLoadOp;
        depthStoreOp = (int)d->depthStoreOp;
        depthClearValue = d->depthClearValue;
        depthReadOnly = (int)d->depthReadOnly;
        stencilLoadOp = (int)d->stencilLoadOp;
        stencilStoreOp = (int)d->stencilStoreOp;
        stencilClearValue = (int)d->stencilClearValue;
        stencilReadOnly = (int)d->stencilReadOnly;
    }
    return (WGPURenderPassEncoder)__wgpu_command_encoder_begin_render_pass(
        (int)commandEncoder, ca, can, cc,
        depthView, depthLoadOp, depthStoreOp, depthClearValue, depthReadOnly,
        stencilLoadOp, stencilStoreOp, stencilClearValue, stencilReadOnly);
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference) {
    __wgpu_render_pass_set_stencil_reference((int)renderPassEncoder, (int)reference);
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline) {
    __wgpu_render_pass_set_pipeline((int)renderPassEncoder, (int)pipeline);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    __wgpu_render_pass_draw((int)renderPassEncoder, (int)vertexCount, (int)instanceCount, (int)firstVertex, (int)firstInstance);
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder) {
    __wgpu_render_pass_end((int)renderPassEncoder);
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor *descriptor) {
    (void)descriptor;
    return (WGPUCommandBuffer)__wgpu_command_encoder_finish((int)commandEncoder);
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer *commands) {
    for (size_t i = 0; i < commandCount; i++)
        __wgpu_queue_submit_one((int)queue, (int)commands[i]);
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor *descriptor) {
    return (WGPUBuffer)__wgpu_device_create_buffer(
        (int)device, (int)descriptor->size, (int)descriptor->usage,
        (int)descriptor->mappedAtCreation);
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset,
        const void *data, size_t size) {
    __wgpu_queue_write_buffer((int)queue, (int)buffer, (int)bufferOffset, data, (int)size);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder,
        uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    __wgpu_render_pass_set_vertex_buffer((int)renderPassEncoder, (int)slot, (int)buffer, (int)offset, (int)size);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder,
        WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    __wgpu_render_pass_set_index_buffer((int)renderPassEncoder, (int)buffer, (int)format, (int)offset, (int)size);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder,
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
    __wgpu_render_pass_draw_indexed((int)renderPassEncoder, (int)indexCount, (int)instanceCount, (int)firstIndex, (int)baseVertex, (int)firstInstance);
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor *desc) {
    /* Packed: [ entryCount, per entry: binding, visibility, kind, detail, e0, e1, e2 ].
       kind 0=buffer (detail=WGPUBufferBindingType, e0=hasDynamicOffset),
       1=sampler (detail=WGPUSamplerBindingType),
       2=texture (detail=WGPUTextureSampleType, e0=viewDimension, e1=multisampled),
       3=storageTexture (detail=WGPUStorageTextureAccess, e0=format, e1=viewDimension).
       The set sub-struct selects kind. */
    static int p[256];
    int n = 0, cap = (int)(sizeof(p) / sizeof(p[0]));
    int ec = (int)desc->entryCount;
    p[n++] = ec;
    for (int i = 0; i < ec; i++) {
        const WGPUBindGroupLayoutEntry *e = &desc->entries[i];
        int kind, detail, e0 = 0, e1 = 0, e2 = 0;
        if (e->buffer.type != WGPUBufferBindingType_Undefined) {
            kind = 0; detail = (int)e->buffer.type; e0 = (int)e->buffer.hasDynamicOffset;
        } else if (e->sampler.type != WGPUSamplerBindingType_Undefined) {
            kind = 1; detail = (int)e->sampler.type;
        } else if (e->texture.sampleType != WGPUTextureSampleType_Undefined) {
            kind = 2; detail = (int)e->texture.sampleType;
            e0 = (int)e->texture.viewDimension; e1 = (int)e->texture.multisampled;
        } else if (e->storageTexture.format != WGPUTextureFormat_Undefined) {
            kind = 3; detail = (int)e->storageTexture.access;
            e0 = (int)e->storageTexture.format; e1 = (int)e->storageTexture.viewDimension;
        } else {
            kind = 0; detail = (int)WGPUBufferBindingType_Uniform;
        }
        if (n + 7 > cap) { fprintf(stderr, "wgpuDeviceCreateBindGroupLayout: entry cap exceeded\n"); abort(); }
        p[n++] = (int)e->binding;
        p[n++] = (int)e->visibility;
        p[n++] = kind;
        p[n++] = detail;
        p[n++] = e0;
        p[n++] = e1;
        p[n++] = e2;
    }
    return (WGPUBindGroupLayout)__wgpu_device_create_bind_group_layout((int)device, p, n);
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor *desc) {
    static int bgls[64];
    int c = (int)desc->bindGroupLayoutCount;
    if (c > (int)(sizeof(bgls) / sizeof(bgls[0]))) { fprintf(stderr, "wgpuDeviceCreatePipelineLayout: too many bind group layouts\n"); abort(); }
    for (int i = 0; i < c; i++) bgls[i] = (int)desc->bindGroupLayouts[i];
    return (WGPUPipelineLayout)__wgpu_device_create_pipeline_layout((int)device, bgls, c);
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor *desc) {
    /* Packed: [ entryCount, per entry: binding, kind, handle, offset, size ].
       kind 0=buffer (handle=buffer, offset, size), 1=sampler (handle=sampler),
       2=textureView (handle=textureView). */
    static int p[256];
    int n = 0, cap = (int)(sizeof(p) / sizeof(p[0]));
    int ec = (int)desc->entryCount;
    p[n++] = ec;
    for (int i = 0; i < ec; i++) {
        const WGPUBindGroupEntry *e = &desc->entries[i];
        int kind, handle, offset = 0, size = 0;
        if (e->buffer) { kind = 0; handle = (int)e->buffer; offset = (int)e->offset; size = (int)e->size; }
        else if (e->sampler) { kind = 1; handle = (int)e->sampler; }
        else if (e->textureView) { kind = 2; handle = (int)e->textureView; }
        else { fprintf(stderr, "wgpuDeviceCreateBindGroup: entry %d has no resource\n", i); abort(); }
        if (n + 5 > cap) { fprintf(stderr, "wgpuDeviceCreateBindGroup: entry cap exceeded\n"); abort(); }
        p[n++] = (int)e->binding;
        p[n++] = kind;
        p[n++] = handle;
        p[n++] = offset;
        p[n++] = size;
    }
    return (WGPUBindGroup)__wgpu_device_create_bind_group((int)device, (int)desc->layout, p, n);
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder,
        uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t *dynamicOffsets) {
    __wgpu_render_pass_set_bind_group((int)renderPassEncoder, (int)groupIndex, (int)group,
        (const int *)dynamicOffsets, (int)dynamicOffsetCount);
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor *d) {
    return (WGPUTexture)__wgpu_device_create_texture((int)device,
        (int)d->size.width, (int)d->size.height, (int)d->size.depthOrArrayLayers,
        (int)d->format, (int)d->usage, (int)d->dimension,
        (int)(d->mipLevelCount ? d->mipLevelCount : 1),
        (int)(d->sampleCount ? d->sampleCount : 1));
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor *d) {
    return (WGPUSampler)__wgpu_device_create_sampler((int)device,
        (int)d->addressModeU, (int)d->addressModeV, (int)d->addressModeW,
        (int)d->magFilter, (int)d->minFilter, (int)d->mipmapFilter,
        (double)d->lodMinClamp, (double)d->lodMaxClamp, (int)d->maxAnisotropy, (int)d->compare);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo *dst,
        const void *data, size_t dataSize, const WGPUTexelCopyBufferLayout *layout,
        const WGPUExtent3D *size) {
    __wgpu_queue_write_texture((int)queue, (int)dst->texture, (int)dst->mipLevel,
        (int)dst->origin.x, (int)dst->origin.y, (int)dst->origin.z, (int)dst->aspect,
        data, (int)dataSize, (int)layout->offset, (int)layout->bytesPerRow, (int)layout->rowsPerImage,
        (int)size->width, (int)size->height, (int)size->depthOrArrayLayers);
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder enc,
        const WGPUTexelCopyTextureInfo *src, const WGPUTexelCopyBufferInfo *dst, const WGPUExtent3D *copySize) {
    __wgpu_cmd_copy_texture_to_buffer((int)enc, (int)src->texture, (int)src->mipLevel,
        (int)src->origin.x, (int)src->origin.y, (int)src->origin.z,
        (int)dst->buffer, (int)dst->layout.offset, (int)dst->layout.bytesPerRow, (int)dst->layout.rowsPerImage,
        (int)copySize->width, (int)copySize->height, (int)copySize->depthOrArrayLayers);
}

WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size,
        WGPUBufferMapCallbackInfo callbackInfo) {
    __wgpu_buffer_map_async((int)buffer, (int)mode, (int)offset, (int)size,
        callbackInfo.callback, callbackInfo.userdata1, callbackInfo.userdata2);
    WGPUFuture f; f.id = ++__wgpu_future_seq; return f;
}

/* Mapped-range staging: getMappedRange mallocs a wasm-side copy of the mapped
   GPU bytes (host fills it via __wgpu_buffer_get_mapped_range for the read
   path). The host also remembers the JS range so wgpuBufferUnmap flushes the
   staging bytes back into the GPU buffer (write path / mappedAtCreation) before
   the staging is freed. */
#define __WGPU_MAX_MAPPED 16
static struct { int buffer; void *ptr; } __wgpu_mapped[__WGPU_MAX_MAPPED];

void *wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
    /* WGPU_WHOLE_SIZE / WGPU_WHOLE_MAP_SIZE truncate to (size_t)-1 in ILP32:
       resolve to "rest of buffer" BEFORE allocating the staging copy (the host
       side gets a concrete size, mirroring the setVertex/IndexBuffer paths). */
    if (size == (size_t)-1) size = (size_t)__wgpu_buffer_get_size((int)buffer) - offset;
    void *p = malloc(size ? size : 1);
    if (!p) { fprintf(stderr, "wgpuBufferGetMappedRange: out of memory\n"); abort(); }
    __wgpu_buffer_get_mapped_range((int)buffer, (int)offset, (int)size, p);
    for (int i = 0; i < __WGPU_MAX_MAPPED; i++) {
        if (!__wgpu_mapped[i].ptr) { __wgpu_mapped[i].buffer = (int)buffer; __wgpu_mapped[i].ptr = p; return p; }
    }
    fprintf(stderr, "wgpuBufferGetMappedRange: too many mapped buffers (raise __WGPU_MAX_MAPPED)\n");
    abort();
}

const void *wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
    return wgpuBufferGetMappedRange(buffer, offset, size);
}

void wgpuBufferUnmap(WGPUBuffer buffer) {
    /* Unmap FIRST: the host flushes the wasm staging bytes back into the GPU
       mapped range (write path / mappedAtCreation) before unmapping. Only then
       is it safe to free the staging copy. */
    __wgpu_buffer_unmap((int)buffer);
    for (int i = 0; i < __WGPU_MAX_MAPPED; i++) {
        if (__wgpu_mapped[i].ptr && __wgpu_mapped[i].buffer == (int)buffer) {
            free(__wgpu_mapped[i].ptr);
            __wgpu_mapped[i].ptr = 0;
            __wgpu_mapped[i].buffer = 0;
        }
    }
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder enc, WGPUBuffer src,
        uint64_t srcOffset, WGPUBuffer dst, uint64_t dstOffset, uint64_t size) {
    __wgpu_cmd_copy_buffer_to_buffer((int)enc, (int)src, (int)srcOffset, (int)dst, (int)dstOffset, (int)size);
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor *desc) {
    static int csc[1 + 64 * 2]; static double cscv[64];
    int cscn = __wgpu_pack_constants(desc->compute.constants, (int)desc->compute.constantCount, csc, cscv, (int)(sizeof(csc) / sizeof(csc[0])));
    return (WGPUComputePipeline)__wgpu_device_create_compute_pipeline((int)device,
        (int)desc->compute.module, desc->compute.entryPoint.data, (int)desc->compute.entryPoint.length,
        (int)desc->layout, csc, cscn, cscv);
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder enc, const WGPUComputePassDescriptor *desc) {
    (void)desc;
    return (WGPUComputePassEncoder)__wgpu_command_encoder_begin_compute_pass((int)enc);
}

void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder pass, WGPUComputePipeline pipeline) {
    __wgpu_compute_pass_set_pipeline((int)pass, (int)pipeline);
}

void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder pass, uint32_t groupIndex,
        WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t *dynamicOffsets) {
    __wgpu_compute_pass_set_bind_group((int)pass, (int)groupIndex, (int)group,
        (const int *)dynamicOffsets, (int)dynamicOffsetCount);
}

void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder pass,
        uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) {
    __wgpu_compute_pass_dispatch((int)pass, (int)workgroupCountX, (int)workgroupCountY, (int)workgroupCountZ);
}

void wgpuComputePassEncoderEnd(WGPUComputePassEncoder pass) {
    __wgpu_compute_pass_end((int)pass);
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter) {
    __wgpu_device_push_error_scope((int)device, (int)filter);
}

WGPUFuture wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo) {
    __wgpu_device_pop_error_scope((int)device, callbackInfo.callback, callbackInfo.userdata1, callbackInfo.userdata2);
    WGPUFuture f; f.id = ++__wgpu_future_seq; return f;
}

void wgpuInstanceRelease(WGPUInstance v) { __wgpu_release((int)v); }
void wgpuAdapterRelease(WGPUAdapter v) { __wgpu_release((int)v); }
void wgpuDeviceRelease(WGPUDevice v) { __wgpu_release((int)v); }
void wgpuQueueRelease(WGPUQueue v) { __wgpu_release((int)v); }
void wgpuSurfaceRelease(WGPUSurface v) { __wgpu_release((int)v); }
void wgpuTextureRelease(WGPUTexture v) { __wgpu_release((int)v); }
void wgpuTextureViewRelease(WGPUTextureView v) { __wgpu_release((int)v); }
void wgpuShaderModuleRelease(WGPUShaderModule v) { __wgpu_release((int)v); }
void wgpuRenderPipelineRelease(WGPURenderPipeline v) { __wgpu_release((int)v); }
void wgpuCommandEncoderRelease(WGPUCommandEncoder v) { __wgpu_release((int)v); }
void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder v) { __wgpu_release((int)v); }
void wgpuCommandBufferRelease(WGPUCommandBuffer v) { __wgpu_release((int)v); }
void wgpuBufferRelease(WGPUBuffer v) { __wgpu_release((int)v); }
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout v) { __wgpu_release((int)v); }
void wgpuBindGroupRelease(WGPUBindGroup v) { __wgpu_release((int)v); }
void wgpuPipelineLayoutRelease(WGPUPipelineLayout v) { __wgpu_release((int)v); }
void wgpuSamplerRelease(WGPUSampler v) { __wgpu_release((int)v); }
void wgpuComputePipelineRelease(WGPUComputePipeline v) { __wgpu_release((int)v); }
void wgpuComputePassEncoderRelease(WGPUComputePassEncoder v) { __wgpu_release((int)v); }

void wgpuSetMainLoopCallback(void (*callback)(void)) {
    __sdl_set_animation_frame_func(callback);
}
  