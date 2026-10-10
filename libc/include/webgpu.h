
#pragma once
/* WebGPU for this compiler's web backend (modern Dawn/Emdawnwebgpu dialect).
   The browser's WebGPU JS API is exposed to C. webgpu.h declares the standard
   types + wgpu* prototypes; __webgpu.c flattens descriptor structs (the C
   compiler's layout is authoritative) and forwards PRIMITIVES to host.js's
   __wgpu_* imports — so the host never hand-computes C struct offsets, mirroring
   __SDL.c's "host knows nothing about C struct layouts" design. Async
   (requestAdapter/requestDevice) is callback-based (NO JSPI): the host invokes
   C trampolines (__wgpu_call_*_cb) which reconstruct the by-value WGPUStringView
   and call the user callback. Frames run on the shared rAF loop via
   wgpuSetMainLoopCallback. See docs/WEBGPU.md. */
__require_source("__webgpu.c");

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#ifndef __WGPU_BOOL_DEFINED
#define __WGPU_BOOL_DEFINED
#endif
#endif

typedef uint32_t WGPUFlags;
typedef uint32_t WGPUBool;

/* Opaque handles (host-side handle-table indices, pointer-width). */
typedef struct WGPUInstanceImpl*          WGPUInstance;
typedef struct WGPUAdapterImpl*           WGPUAdapter;
typedef struct WGPUDeviceImpl*            WGPUDevice;
typedef struct WGPUQueueImpl*             WGPUQueue;
typedef struct WGPUSurfaceImpl*           WGPUSurface;
typedef struct WGPUTextureImpl*           WGPUTexture;
typedef struct WGPUTextureViewImpl*       WGPUTextureView;
typedef struct WGPUShaderModuleImpl*      WGPUShaderModule;
typedef struct WGPUPipelineLayoutImpl*    WGPUPipelineLayout;
typedef struct WGPURenderPipelineImpl*    WGPURenderPipeline;
typedef struct WGPUCommandEncoderImpl*    WGPUCommandEncoder;
typedef struct WGPURenderPassEncoderImpl* WGPURenderPassEncoder;
typedef struct WGPUCommandBufferImpl*     WGPUCommandBuffer;
typedef struct WGPUBufferImpl*            WGPUBuffer;
typedef struct WGPUBindGroupLayoutImpl*   WGPUBindGroupLayout;
typedef struct WGPUBindGroupImpl*         WGPUBindGroup;
typedef struct WGPUSamplerImpl*           WGPUSampler;
typedef struct WGPUComputePipelineImpl*   WGPUComputePipeline;
typedef struct WGPUComputePassEncoderImpl* WGPUComputePassEncoder;

/* WGPUStringView: ptr+len string (NULL data + 0 length == "use default"). */
typedef struct WGPUStringView {
    const char *data;
    size_t length;
} WGPUStringView;
#define WGPU_STRLEN ((size_t)-1)
#define WGPU_WHOLE_SIZE (0xFFFFFFFFFFFFFFFFULL)
#define WGPU_WHOLE_MAP_SIZE ((size_t)-1)

/* WGPUFuture: opaque async token. We drive completion via callbacks, so the id
   is a plain monotonic counter the user does not need to inspect. */
typedef struct WGPUFuture { uint64_t id; } WGPUFuture;

typedef struct WGPUColor { double r, g, b, a; } WGPUColor;

/* ---- Enums (values are self-consistent header<->host; use the NAMES). ---- */
typedef enum WGPUSType {
    WGPUSType_ShaderSourceWGSL = 2
} WGPUSType;

typedef enum WGPURequestAdapterStatus {
    WGPURequestAdapterStatus_Success = 1,
    WGPURequestAdapterStatus_Unavailable = 2,
    WGPURequestAdapterStatus_Error = 3
} WGPURequestAdapterStatus;

typedef enum WGPURequestDeviceStatus {
    WGPURequestDeviceStatus_Success = 1,
    WGPURequestDeviceStatus_Error = 3
} WGPURequestDeviceStatus;

typedef enum WGPUCallbackMode {
    WGPUCallbackMode_WaitAnyOnly = 1,
    WGPUCallbackMode_AllowProcessEvents = 2,
    WGPUCallbackMode_AllowSpontaneous = 3
} WGPUCallbackMode;

typedef enum WGPUPowerPreference {
    WGPUPowerPreference_Undefined = 0,
    WGPUPowerPreference_LowPower = 1,
    WGPUPowerPreference_HighPerformance = 2
} WGPUPowerPreference;

/* Full WebGPU texture format set. Uncompressed formats use canonical webgpu.h
   values (1-44); the 9 historical entries (18/19/23/24, 40-44) are unchanged.
   Compressed formats (feature-gated: texture-compression-bc/etc2/astc) live at
   100+ and resolve only when the adapter exposes the feature. The host map is
   the single source of truth (int->string); unknown values fail loud. */
typedef enum WGPUTextureFormat {
    WGPUTextureFormat_Undefined            = 0,
    WGPUTextureFormat_R8Unorm              = 1,
    WGPUTextureFormat_R8Snorm              = 2,
    WGPUTextureFormat_R8Uint               = 3,
    WGPUTextureFormat_R8Sint               = 4,
    WGPUTextureFormat_R16Uint              = 5,
    WGPUTextureFormat_R16Sint              = 6,
    WGPUTextureFormat_R16Float             = 7,
    WGPUTextureFormat_RG8Unorm             = 8,
    WGPUTextureFormat_RG8Snorm             = 9,
    WGPUTextureFormat_RG8Uint              = 10,
    WGPUTextureFormat_RG8Sint              = 11,
    WGPUTextureFormat_R32Float             = 12,
    WGPUTextureFormat_R32Uint              = 13,
    WGPUTextureFormat_R32Sint              = 14,
    WGPUTextureFormat_RG16Uint             = 15,
    WGPUTextureFormat_RG16Sint             = 16,
    WGPUTextureFormat_RG16Float            = 17,
    WGPUTextureFormat_RGBA8Unorm           = 18,
    WGPUTextureFormat_RGBA8UnormSrgb       = 19,
    WGPUTextureFormat_RGBA8Snorm           = 20,
    WGPUTextureFormat_RGBA8Uint            = 21,
    WGPUTextureFormat_RGBA8Sint            = 22,
    WGPUTextureFormat_BGRA8Unorm           = 23,
    WGPUTextureFormat_BGRA8UnormSrgb       = 24,
    WGPUTextureFormat_RGB10A2Uint          = 25,
    WGPUTextureFormat_RGB10A2Unorm         = 26,
    WGPUTextureFormat_RG11B10Ufloat        = 27,
    WGPUTextureFormat_RGB9E5Ufloat         = 28,
    WGPUTextureFormat_RG32Float            = 29,
    WGPUTextureFormat_RG32Uint             = 30,
    WGPUTextureFormat_RG32Sint             = 31,
    WGPUTextureFormat_RGBA16Uint           = 32,
    WGPUTextureFormat_RGBA16Sint           = 33,
    WGPUTextureFormat_RGBA16Float          = 34,
    WGPUTextureFormat_RGBA32Float          = 35,
    WGPUTextureFormat_RGBA32Uint           = 36,
    WGPUTextureFormat_RGBA32Sint           = 37,
    WGPUTextureFormat_Stencil8             = 38,
    WGPUTextureFormat_Depth16Unorm         = 40,
    WGPUTextureFormat_Depth24Plus          = 41,
    WGPUTextureFormat_Depth24PlusStencil8  = 42,
    WGPUTextureFormat_Depth32Float         = 43,
    WGPUTextureFormat_Depth32FloatStencil8 = 44,
    /* --- compressed (feature-gated) --- */
    WGPUTextureFormat_BC1RGBAUnorm         = 100,
    WGPUTextureFormat_BC1RGBAUnormSrgb     = 101,
    WGPUTextureFormat_BC2RGBAUnorm         = 102,
    WGPUTextureFormat_BC2RGBAUnormSrgb     = 103,
    WGPUTextureFormat_BC3RGBAUnorm         = 104,
    WGPUTextureFormat_BC3RGBAUnormSrgb     = 105,
    WGPUTextureFormat_BC4RUnorm            = 106,
    WGPUTextureFormat_BC4RSnorm            = 107,
    WGPUTextureFormat_BC5RGUnorm           = 108,
    WGPUTextureFormat_BC5RGSnorm           = 109,
    WGPUTextureFormat_BC6HRGBUfloat        = 110,
    WGPUTextureFormat_BC6HRGBFloat         = 111,
    WGPUTextureFormat_BC7RGBAUnorm         = 112,
    WGPUTextureFormat_BC7RGBAUnormSrgb     = 113,
    WGPUTextureFormat_ETC2RGB8Unorm        = 114,
    WGPUTextureFormat_ETC2RGB8UnormSrgb    = 115,
    WGPUTextureFormat_ETC2RGB8A1Unorm      = 116,
    WGPUTextureFormat_ETC2RGB8A1UnormSrgb  = 117,
    WGPUTextureFormat_ETC2RGBA8Unorm       = 118,
    WGPUTextureFormat_ETC2RGBA8UnormSrgb   = 119,
    WGPUTextureFormat_EACR11Unorm          = 120,
    WGPUTextureFormat_EACR11Snorm          = 121,
    WGPUTextureFormat_EACRG11Unorm         = 122,
    WGPUTextureFormat_EACRG11Snorm         = 123,
    WGPUTextureFormat_ASTC4x4Unorm         = 124,
    WGPUTextureFormat_ASTC4x4UnormSrgb     = 125,
    WGPUTextureFormat_ASTC5x4Unorm         = 126,
    WGPUTextureFormat_ASTC5x4UnormSrgb     = 127,
    WGPUTextureFormat_ASTC5x5Unorm         = 128,
    WGPUTextureFormat_ASTC5x5UnormSrgb     = 129,
    WGPUTextureFormat_ASTC6x5Unorm         = 130,
    WGPUTextureFormat_ASTC6x5UnormSrgb     = 131,
    WGPUTextureFormat_ASTC6x6Unorm         = 132,
    WGPUTextureFormat_ASTC6x6UnormSrgb     = 133,
    WGPUTextureFormat_ASTC8x5Unorm         = 134,
    WGPUTextureFormat_ASTC8x5UnormSrgb     = 135,
    WGPUTextureFormat_ASTC8x6Unorm         = 136,
    WGPUTextureFormat_ASTC8x6UnormSrgb     = 137,
    WGPUTextureFormat_ASTC8x8Unorm         = 138,
    WGPUTextureFormat_ASTC8x8UnormSrgb     = 139,
    WGPUTextureFormat_ASTC10x5Unorm        = 140,
    WGPUTextureFormat_ASTC10x5UnormSrgb    = 141,
    WGPUTextureFormat_ASTC10x6Unorm        = 142,
    WGPUTextureFormat_ASTC10x6UnormSrgb    = 143,
    WGPUTextureFormat_ASTC10x8Unorm        = 144,
    WGPUTextureFormat_ASTC10x8UnormSrgb    = 145,
    WGPUTextureFormat_ASTC10x10Unorm       = 146,
    WGPUTextureFormat_ASTC10x10UnormSrgb   = 147,
    WGPUTextureFormat_ASTC12x10Unorm       = 148,
    WGPUTextureFormat_ASTC12x10UnormSrgb   = 149,
    WGPUTextureFormat_ASTC12x12Unorm       = 150,
    WGPUTextureFormat_ASTC12x12UnormSrgb   = 151
} WGPUTextureFormat;

typedef enum WGPULoadOp {
    WGPULoadOp_Undefined = 0,
    WGPULoadOp_Clear = 1,
    WGPULoadOp_Load = 2
} WGPULoadOp;

typedef enum WGPUStoreOp {
    WGPUStoreOp_Undefined = 0,
    WGPUStoreOp_Store = 1,
    WGPUStoreOp_Discard = 2
} WGPUStoreOp;

typedef enum WGPUPrimitiveTopology {
    WGPUPrimitiveTopology_PointList = 0,
    WGPUPrimitiveTopology_LineList = 1,
    WGPUPrimitiveTopology_LineStrip = 2,
    WGPUPrimitiveTopology_TriangleList = 3,
    WGPUPrimitiveTopology_TriangleStrip = 4
} WGPUPrimitiveTopology;

typedef enum WGPUFrontFace {
    WGPUFrontFace_CCW = 0,
    WGPUFrontFace_CW = 1
} WGPUFrontFace;

typedef enum WGPUCullMode {
    WGPUCullMode_None = 0,
    WGPUCullMode_Front = 1,
    WGPUCullMode_Back = 2
} WGPUCullMode;

typedef enum WGPUIndexFormat {
    WGPUIndexFormat_Undefined = 0,
    WGPUIndexFormat_Uint16 = 1,
    WGPUIndexFormat_Uint32 = 2
} WGPUIndexFormat;

typedef enum WGPUCompositeAlphaMode {
    WGPUCompositeAlphaMode_Auto = 0,
    WGPUCompositeAlphaMode_Opaque = 1,
    WGPUCompositeAlphaMode_Premultiplied = 2
} WGPUCompositeAlphaMode;

typedef enum WGPUPresentMode {
    WGPUPresentMode_Undefined = 0,
    WGPUPresentMode_Fifo = 1,
    WGPUPresentMode_FifoRelaxed = 2,
    WGPUPresentMode_Immediate = 3,
    WGPUPresentMode_Mailbox = 4
} WGPUPresentMode;

/* Vertex attribute formats. Values are self-consistent header<->host (the host
   maps these ints to WGSL format strings). Values 1-9 are historical (kept for
   compatibility); 10+ append the rest of the WebGPU set. The NAME is the
   contract — these ints are NOT the canonical Dawn webgpu.h numbers. Full
   coverage: every WGPUVertexFormat the spec defines is mapped host-side. */
typedef enum WGPUVertexFormat {
    WGPUVertexFormat_Undefined  = 0,
    WGPUVertexFormat_Float32    = 1,
    WGPUVertexFormat_Float32x2  = 2,
    WGPUVertexFormat_Float32x3  = 3,
    WGPUVertexFormat_Float32x4  = 4,
    WGPUVertexFormat_Uint32     = 5,
    WGPUVertexFormat_Uint32x2   = 6,
    WGPUVertexFormat_Uint32x3   = 7,
    WGPUVertexFormat_Uint32x4   = 8,
    WGPUVertexFormat_Unorm8x4   = 9,
    /* --- appended: rest of the WebGPU vertex format set --- */
    WGPUVertexFormat_Uint8      = 10,
    WGPUVertexFormat_Uint8x2    = 11,
    WGPUVertexFormat_Uint8x4    = 12,
    WGPUVertexFormat_Sint8      = 13,
    WGPUVertexFormat_Sint8x2    = 14,
    WGPUVertexFormat_Sint8x4    = 15,
    WGPUVertexFormat_Unorm8     = 16,
    WGPUVertexFormat_Unorm8x2   = 17,
    WGPUVertexFormat_Snorm8     = 18,
    WGPUVertexFormat_Snorm8x2   = 19,
    WGPUVertexFormat_Snorm8x4   = 20,
    WGPUVertexFormat_Uint16     = 21,
    WGPUVertexFormat_Uint16x2   = 22,
    WGPUVertexFormat_Uint16x4   = 23,
    WGPUVertexFormat_Sint16     = 24,
    WGPUVertexFormat_Sint16x2   = 25,
    WGPUVertexFormat_Sint16x4   = 26,
    WGPUVertexFormat_Unorm16    = 27,
    WGPUVertexFormat_Unorm16x2  = 28,
    WGPUVertexFormat_Unorm16x4  = 29,
    WGPUVertexFormat_Snorm16    = 30,
    WGPUVertexFormat_Snorm16x2  = 31,
    WGPUVertexFormat_Snorm16x4  = 32,
    WGPUVertexFormat_Float16    = 33,
    WGPUVertexFormat_Float16x2  = 34,
    WGPUVertexFormat_Float16x4  = 35,
    WGPUVertexFormat_Sint32     = 36,
    WGPUVertexFormat_Sint32x2   = 37,
    WGPUVertexFormat_Sint32x3   = 38,
    WGPUVertexFormat_Sint32x4   = 39,
    WGPUVertexFormat_Unorm10_10_10_2 = 40,
    WGPUVertexFormat_Unorm8x4BGRA    = 41
} WGPUVertexFormat;

typedef enum WGPUVertexStepMode {
    WGPUVertexStepMode_Vertex   = 0,
    WGPUVertexStepMode_Instance = 1
} WGPUVertexStepMode;

/* Buffer usage flags. Values mirror the JS GPUBufferUsage bits exactly so they
   pass straight through to the host (no remap). */
#define WGPUBufferUsage_MapRead      0x0001
#define WGPUBufferUsage_MapWrite     0x0002
#define WGPUBufferUsage_CopySrc      0x0004
#define WGPUBufferUsage_CopyDst      0x0008
#define WGPUBufferUsage_Index        0x0010
#define WGPUBufferUsage_Vertex       0x0020
#define WGPUBufferUsage_Uniform      0x0040
#define WGPUBufferUsage_Storage      0x0080
#define WGPUBufferUsage_Indirect     0x0100
#define WGPUBufferUsage_QueryResolve 0x0200

/* Shader stage visibility flags (mirror JS GPUShaderStage). */
#define WGPUShaderStage_Vertex   0x1
#define WGPUShaderStage_Fragment 0x2
#define WGPUShaderStage_Compute  0x4

typedef enum WGPUBufferBindingType {
    WGPUBufferBindingType_Undefined        = 0,
    WGPUBufferBindingType_Uniform          = 1,
    WGPUBufferBindingType_Storage          = 2,
    WGPUBufferBindingType_ReadOnlyStorage  = 3
} WGPUBufferBindingType;

/* Texture usage flags. */
#define WGPUTextureUsage_CopySrc          0x01
#define WGPUTextureUsage_CopyDst          0x02
#define WGPUTextureUsage_TextureBinding   0x04
#define WGPUTextureUsage_StorageBinding   0x08
#define WGPUTextureUsage_RenderAttachment 0x10

/* Color write mask. */
#define WGPUColorWriteMask_None  0x0
#define WGPUColorWriteMask_Red   0x1
#define WGPUColorWriteMask_Green 0x2
#define WGPUColorWriteMask_Blue  0x4
#define WGPUColorWriteMask_Alpha 0x8
#define WGPUColorWriteMask_All   0xF

#define WGPU_DEPTH_SLICE_UNDEFINED 0xFFFFFFFF

/* ---- Chained struct base ---- */
typedef struct WGPUChainedStruct {
    const struct WGPUChainedStruct *next;
    WGPUSType sType;
} WGPUChainedStruct;

/* ---- Callbacks + callback-info ---- */
typedef void (*WGPURequestAdapterCallback)(WGPURequestAdapterStatus status,
    WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2);
typedef void (*WGPURequestDeviceCallback)(WGPURequestDeviceStatus status,
    WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2);

typedef struct WGPURequestAdapterCallbackInfo {
    const WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPURequestAdapterCallback callback;
    void *userdata1;
    void *userdata2;
} WGPURequestAdapterCallbackInfo;

typedef struct WGPURequestDeviceCallbackInfo {
    const WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPURequestDeviceCallback callback;
    void *userdata1;
    void *userdata2;
} WGPURequestDeviceCallbackInfo;

/* ---- Descriptors ---- */
typedef struct WGPUInstanceDescriptor {
    const WGPUChainedStruct *nextInChain;
} WGPUInstanceDescriptor;

typedef struct WGPURequestAdapterOptions {
    const WGPUChainedStruct *nextInChain;
    WGPUSurface compatibleSurface;
    WGPUPowerPreference powerPreference;
    WGPUBool forceFallbackAdapter;
} WGPURequestAdapterOptions;

typedef struct WGPUDeviceDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUDeviceDescriptor;

typedef struct WGPUSurfaceDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUSurfaceDescriptor;

typedef struct WGPUSurfaceConfiguration {
    const WGPUChainedStruct *nextInChain;
    WGPUDevice device;
    WGPUTextureFormat format;
    WGPUFlags usage;
    uint32_t width;
    uint32_t height;
    size_t viewFormatCount;
    const WGPUTextureFormat *viewFormats;
    WGPUCompositeAlphaMode alphaMode;
    WGPUPresentMode presentMode;
} WGPUSurfaceConfiguration;

typedef enum WGPUSurfaceGetCurrentTextureStatus {
    WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal = 1,
    WGPUSurfaceGetCurrentTextureStatus_Error = 5
} WGPUSurfaceGetCurrentTextureStatus;

typedef struct WGPUSurfaceTexture {
    const WGPUChainedStruct *nextInChain;
    WGPUTexture texture;
    WGPUSurfaceGetCurrentTextureStatus status;
} WGPUSurfaceTexture;

typedef struct WGPUShaderSourceWGSL {
    WGPUChainedStruct chain;   /* chain.sType = WGPUSType_ShaderSourceWGSL */
    WGPUStringView code;
} WGPUShaderSourceWGSL;

typedef struct WGPUShaderModuleDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUShaderModuleDescriptor;

/* WGPUTextureViewDescriptor is defined after the texture enums it references
   (WGPUTextureViewDimension / WGPUTextureAspect), below near WGPUSamplerDescriptor. */
typedef struct WGPUTextureViewDescriptor WGPUTextureViewDescriptor;

typedef enum WGPUBlendOperation {
    WGPUBlendOperation_Add = 1,
    WGPUBlendOperation_Subtract = 2,
    WGPUBlendOperation_ReverseSubtract = 3,
    WGPUBlendOperation_Min = 4,
    WGPUBlendOperation_Max = 5
} WGPUBlendOperation;

typedef enum WGPUBlendFactor {
    WGPUBlendFactor_Zero = 0,
    WGPUBlendFactor_One = 1,
    WGPUBlendFactor_Src = 2,
    WGPUBlendFactor_OneMinusSrc = 3,
    WGPUBlendFactor_SrcAlpha = 4,
    WGPUBlendFactor_OneMinusSrcAlpha = 5,
    WGPUBlendFactor_Dst = 6,
    WGPUBlendFactor_OneMinusDst = 7,
    WGPUBlendFactor_DstAlpha = 8,
    WGPUBlendFactor_OneMinusDstAlpha = 9
} WGPUBlendFactor;

typedef struct WGPUBlendComponent {
    WGPUBlendOperation operation;
    WGPUBlendFactor srcFactor;
    WGPUBlendFactor dstFactor;
} WGPUBlendComponent;

typedef struct WGPUBlendState {
    WGPUBlendComponent color;
    WGPUBlendComponent alpha;
} WGPUBlendState;

/* Pipeline-overridable constant (WGPUConstantEntry): a WGSL override keyed by
   its name (or numeric id as a string) set to a scalar double at pipeline
   creation. */
typedef struct WGPUConstantEntry {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView key;
    double value;
} WGPUConstantEntry;

typedef struct WGPUVertexAttribute {
    WGPUVertexFormat format;
    uint64_t offset;
    uint32_t shaderLocation;
} WGPUVertexAttribute;

typedef struct WGPUVertexBufferLayout {
    uint64_t arrayStride;
    WGPUVertexStepMode stepMode;
    size_t attributeCount;
    const WGPUVertexAttribute *attributes;
} WGPUVertexBufferLayout;

typedef struct WGPUBufferDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUFlags usage;
    uint64_t size;
    WGPUBool mappedAtCreation;
} WGPUBufferDescriptor;

/* ---- Bind groups / pipeline layout ---- */
typedef enum WGPUSamplerBindingType {
    WGPUSamplerBindingType_Undefined = 0,
    WGPUSamplerBindingType_Filtering = 1,
    WGPUSamplerBindingType_NonFiltering = 2,
    WGPUSamplerBindingType_Comparison = 3
} WGPUSamplerBindingType;

typedef enum WGPUTextureSampleType {
    WGPUTextureSampleType_Undefined = 0,
    WGPUTextureSampleType_Float = 1,
    WGPUTextureSampleType_UnfilterableFloat = 2,
    WGPUTextureSampleType_Depth = 3,
    WGPUTextureSampleType_Sint = 4,
    WGPUTextureSampleType_Uint = 5
} WGPUTextureSampleType;

typedef enum WGPUTextureViewDimension {
    WGPUTextureViewDimension_Undefined = 0,
    WGPUTextureViewDimension_1D = 1,
    WGPUTextureViewDimension_2D = 2,
    WGPUTextureViewDimension_2DArray = 3,
    WGPUTextureViewDimension_Cube = 4,
    WGPUTextureViewDimension_CubeArray = 5,
    WGPUTextureViewDimension_3D = 6
} WGPUTextureViewDimension;

typedef enum WGPUStorageTextureAccess {
    WGPUStorageTextureAccess_Undefined = 0,
    WGPUStorageTextureAccess_WriteOnly = 1,
    WGPUStorageTextureAccess_ReadOnly = 2,
    WGPUStorageTextureAccess_ReadWrite = 3
} WGPUStorageTextureAccess;

typedef struct WGPUBufferBindingLayout {
    const WGPUChainedStruct *nextInChain;
    WGPUBufferBindingType type;
    WGPUBool hasDynamicOffset;
    uint64_t minBindingSize;
} WGPUBufferBindingLayout;

typedef struct WGPUSamplerBindingLayout {
    const WGPUChainedStruct *nextInChain;
    WGPUSamplerBindingType type;
} WGPUSamplerBindingLayout;

typedef struct WGPUTextureBindingLayout {
    const WGPUChainedStruct *nextInChain;
    WGPUTextureSampleType sampleType;
    WGPUTextureViewDimension viewDimension;
    WGPUBool multisampled;
} WGPUTextureBindingLayout;

typedef struct WGPUStorageTextureBindingLayout {
    const WGPUChainedStruct *nextInChain;
    WGPUStorageTextureAccess access;
    WGPUTextureFormat format;
    WGPUTextureViewDimension viewDimension;
} WGPUStorageTextureBindingLayout;

typedef struct WGPUBindGroupLayoutEntry {
    const WGPUChainedStruct *nextInChain;
    uint32_t binding;
    WGPUFlags visibility;
    WGPUBufferBindingLayout buffer;
    WGPUSamplerBindingLayout sampler;
    WGPUTextureBindingLayout texture;
    WGPUStorageTextureBindingLayout storageTexture;
} WGPUBindGroupLayoutEntry;

typedef struct WGPUBindGroupLayoutDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    size_t entryCount;
    const WGPUBindGroupLayoutEntry *entries;
} WGPUBindGroupLayoutDescriptor;

typedef struct WGPUBindGroupEntry {
    const WGPUChainedStruct *nextInChain;
    uint32_t binding;
    WGPUBuffer buffer;
    uint64_t offset;
    uint64_t size;
    WGPUSampler sampler;
    WGPUTextureView textureView;
} WGPUBindGroupEntry;

typedef struct WGPUBindGroupDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUBindGroupLayout layout;
    size_t entryCount;
    const WGPUBindGroupEntry *entries;
} WGPUBindGroupDescriptor;

typedef struct WGPUPipelineLayoutDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    size_t bindGroupLayoutCount;
    const WGPUBindGroupLayout *bindGroupLayouts;
} WGPUPipelineLayoutDescriptor;

/* ---- Textures + samplers ---- */
typedef enum WGPUTextureDimension {
    WGPUTextureDimension_1D = 1,
    WGPUTextureDimension_2D = 2,
    WGPUTextureDimension_3D = 3
} WGPUTextureDimension;

typedef enum WGPUTextureAspect {
    WGPUTextureAspect_Undefined = 0,
    WGPUTextureAspect_All = 1,
    WGPUTextureAspect_StencilOnly = 2,
    WGPUTextureAspect_DepthOnly = 3
} WGPUTextureAspect;

typedef enum WGPUAddressMode {
    WGPUAddressMode_Undefined = 0,
    WGPUAddressMode_ClampToEdge = 1,
    WGPUAddressMode_Repeat = 2,
    WGPUAddressMode_MirrorRepeat = 3
} WGPUAddressMode;

typedef enum WGPUFilterMode {
    WGPUFilterMode_Undefined = 0,
    WGPUFilterMode_Nearest = 1,
    WGPUFilterMode_Linear = 2
} WGPUFilterMode;

typedef enum WGPUMipmapFilterMode {
    WGPUMipmapFilterMode_Undefined = 0,
    WGPUMipmapFilterMode_Nearest = 1,
    WGPUMipmapFilterMode_Linear = 2
} WGPUMipmapFilterMode;

typedef struct WGPUExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
} WGPUExtent3D;

typedef struct WGPUOrigin3D {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} WGPUOrigin3D;

typedef struct WGPUTextureDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUFlags usage;
    WGPUTextureDimension dimension;
    WGPUExtent3D size;
    WGPUTextureFormat format;
    uint32_t mipLevelCount;
    uint32_t sampleCount;
    size_t viewFormatCount;
    const WGPUTextureFormat *viewFormats;
} WGPUTextureDescriptor;

typedef enum WGPUCompareFunction {
    WGPUCompareFunction_Undefined = 0,
    WGPUCompareFunction_Never = 1,
    WGPUCompareFunction_Less = 2,
    WGPUCompareFunction_Equal = 3,
    WGPUCompareFunction_LessEqual = 4,
    WGPUCompareFunction_Greater = 5,
    WGPUCompareFunction_NotEqual = 6,
    WGPUCompareFunction_GreaterEqual = 7,
    WGPUCompareFunction_Always = 8
} WGPUCompareFunction;

typedef struct WGPUSamplerDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUAddressMode addressModeU;
    WGPUAddressMode addressModeV;
    WGPUAddressMode addressModeW;
    WGPUFilterMode magFilter;
    WGPUFilterMode minFilter;
    WGPUMipmapFilterMode mipmapFilter;
    float lodMinClamp;
    float lodMaxClamp;
    WGPUCompareFunction compare;   /* 0 (Undefined) => not a comparison sampler */
    uint16_t maxAnisotropy;
} WGPUSamplerDescriptor;

/* Full texture-view descriptor (defined here, after WGPUTextureViewDimension /
   WGPUTextureAspect / WGPUTextureFormat). 0 fields mean "default": format 0 =
   the texture's format, dimension 0 = inferred, mipLevelCount/arrayLayerCount 0
   = remaining levels/layers. */
typedef struct WGPUTextureViewDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUTextureFormat format;
    WGPUTextureViewDimension dimension;
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
    WGPUTextureAspect aspect;
} WGPUTextureViewDescriptor;

/* Newer dialect spelling: WGPUTexelCopyTextureInfo / WGPUTexelCopyBufferLayout
   (formerly WGPUImageCopyTexture / WGPUTextureDataLayout). */
typedef struct WGPUTexelCopyTextureInfo {
    WGPUTexture texture;
    uint32_t mipLevel;
    WGPUOrigin3D origin;
    WGPUTextureAspect aspect;
} WGPUTexelCopyTextureInfo;

typedef struct WGPUTexelCopyBufferLayout {
    uint64_t offset;
    uint32_t bytesPerRow;
    uint32_t rowsPerImage;
} WGPUTexelCopyBufferLayout;

typedef struct WGPUTexelCopyBufferInfo {
    WGPUTexelCopyBufferLayout layout;
    WGPUBuffer buffer;
} WGPUTexelCopyBufferInfo;

/* ---- Buffer mapping / readback (async; callback-based, NO JSPI) ---- */
typedef WGPUFlags WGPUMapMode;
#define WGPUMapMode_None  0x0
#define WGPUMapMode_Read  0x1
#define WGPUMapMode_Write 0x2

typedef enum WGPUMapAsyncStatus {
    WGPUMapAsyncStatus_Success = 1,
    WGPUMapAsyncStatus_Error = 3,
    WGPUMapAsyncStatus_Aborted = 4
} WGPUMapAsyncStatus;

typedef void (*WGPUBufferMapCallback)(WGPUMapAsyncStatus status,
    WGPUStringView message, void *userdata1, void *userdata2);

typedef struct WGPUBufferMapCallbackInfo {
    const WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUBufferMapCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUBufferMapCallbackInfo;

/* ---- Error handling (push/pop error scope; callback-based, NO JSPI) ---- */
typedef enum WGPUErrorFilter {
    WGPUErrorFilter_Validation = 1,
    WGPUErrorFilter_OutOfMemory = 2,
    WGPUErrorFilter_Internal = 3
} WGPUErrorFilter;

typedef enum WGPUErrorType {
    WGPUErrorType_NoError = 1,
    WGPUErrorType_Validation = 2,
    WGPUErrorType_OutOfMemory = 3,
    WGPUErrorType_Internal = 4,
    WGPUErrorType_Unknown = 5
} WGPUErrorType;

typedef enum WGPUPopErrorScopeStatus {
    WGPUPopErrorScopeStatus_Success = 1,
    WGPUPopErrorScopeStatus_Error = 3
} WGPUPopErrorScopeStatus;

typedef void (*WGPUPopErrorScopeCallback)(WGPUPopErrorScopeStatus status,
    WGPUErrorType type, WGPUStringView message, void *userdata1, void *userdata2);

typedef struct WGPUPopErrorScopeCallbackInfo {
    const WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUPopErrorScopeCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUPopErrorScopeCallbackInfo;

typedef struct WGPUColorTargetState {
    const WGPUChainedStruct *nextInChain;
    WGPUTextureFormat format;
    const WGPUBlendState *blend;
    WGPUFlags writeMask;
} WGPUColorTargetState;

typedef struct WGPUVertexState {
    const WGPUChainedStruct *nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    const WGPUConstantEntry *constants;
    size_t bufferCount;
    const WGPUVertexBufferLayout *buffers;
} WGPUVertexState;

typedef struct WGPUFragmentState {
    const WGPUChainedStruct *nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    const WGPUConstantEntry *constants;
    size_t targetCount;
    const WGPUColorTargetState *targets;
} WGPUFragmentState;

typedef struct WGPUPrimitiveState {
    const WGPUChainedStruct *nextInChain;
    WGPUPrimitiveTopology topology;
    WGPUIndexFormat stripIndexFormat;
    WGPUFrontFace frontFace;
    WGPUCullMode cullMode;
} WGPUPrimitiveState;

typedef struct WGPUMultisampleState {
    const WGPUChainedStruct *nextInChain;
    uint32_t count;
    uint32_t mask;
    WGPUBool alphaToCoverageEnabled;
} WGPUMultisampleState;

typedef enum WGPUStencilOperation {
    WGPUStencilOperation_Keep = 1,
    WGPUStencilOperation_Zero = 2,
    WGPUStencilOperation_Replace = 3,
    WGPUStencilOperation_Invert = 4,
    WGPUStencilOperation_IncrementClamp = 5,
    WGPUStencilOperation_DecrementClamp = 6,
    WGPUStencilOperation_IncrementWrap = 7,
    WGPUStencilOperation_DecrementWrap = 8
} WGPUStencilOperation;

typedef struct WGPUStencilFaceState {
    WGPUCompareFunction compare;
    WGPUStencilOperation failOp;
    WGPUStencilOperation depthFailOp;
    WGPUStencilOperation passOp;
} WGPUStencilFaceState;

/* WGPUOptionalBool-style depthWriteEnabled in the modern dialect; we accept a
   plain WGPUBool (0/1) — 1 = write depth. */
typedef struct WGPUDepthStencilState {
    const WGPUChainedStruct *nextInChain;
    WGPUTextureFormat format;
    WGPUBool depthWriteEnabled;
    WGPUCompareFunction depthCompare;
    WGPUStencilFaceState stencilFront;
    WGPUStencilFaceState stencilBack;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;
    int32_t depthBias;
    float depthBiasSlopeScale;
    float depthBiasClamp;
} WGPUDepthStencilState;

typedef struct WGPURenderPipelineDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUPipelineLayout layout;
    WGPUVertexState vertex;
    WGPUPrimitiveState primitive;
    const WGPUDepthStencilState *depthStencil;
    WGPUMultisampleState multisample;
    const WGPUFragmentState *fragment;
} WGPURenderPipelineDescriptor;

typedef struct WGPUCommandEncoderDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUCommandEncoderDescriptor;

typedef struct WGPURenderPassDepthStencilAttachment {
    WGPUTextureView view;
    WGPULoadOp depthLoadOp;
    WGPUStoreOp depthStoreOp;
    float depthClearValue;
    WGPUBool depthReadOnly;
    WGPULoadOp stencilLoadOp;
    WGPUStoreOp stencilStoreOp;
    uint32_t stencilClearValue;
    WGPUBool stencilReadOnly;
} WGPURenderPassDepthStencilAttachment;

typedef struct WGPURenderPassColorAttachment {
    const WGPUChainedStruct *nextInChain;
    WGPUTextureView view;
    uint32_t depthSlice;
    WGPUTextureView resolveTarget;
    WGPULoadOp loadOp;
    WGPUStoreOp storeOp;
    WGPUColor clearValue;
} WGPURenderPassColorAttachment;

typedef struct WGPURenderPassDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    size_t colorAttachmentCount;
    const WGPURenderPassColorAttachment *colorAttachments;
    const WGPURenderPassDepthStencilAttachment *depthStencilAttachment;
} WGPURenderPassDescriptor;

typedef struct WGPUCommandBufferDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
} WGPUCommandBufferDescriptor;

/* ---- Compute ---- */
typedef struct WGPUComputeState {
    const WGPUChainedStruct *nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    const WGPUConstantEntry *constants;
} WGPUComputeState;

typedef struct WGPUComputePipelineDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUPipelineLayout layout;
    WGPUComputeState compute;
} WGPUComputePipelineDescriptor;

typedef struct WGPUComputePassDescriptor {
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    const void *timestampWrites;   /* WGPUComputePassTimestampWrites*, ignored */
} WGPUComputePassDescriptor;

/* ---- Functions ---- */
WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor *descriptor);
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance,
    const WGPURequestAdapterOptions *options, WGPURequestAdapterCallbackInfo callbackInfo);
WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance,
    const WGPUSurfaceDescriptor *descriptor);
WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter,
    const WGPUDeviceDescriptor *descriptor, WGPURequestDeviceCallbackInfo callbackInfo);
WGPUQueue wgpuDeviceGetQueue(WGPUDevice device);

WGPUTextureFormat wgpuSurfaceGetPreferredFormat(WGPUSurface surface, WGPUAdapter adapter);
void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration *config);
void wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture *surfaceTexture);
void wgpuSurfacePresent(WGPUSurface surface);

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor *descriptor);
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor *descriptor);
WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor *descriptor);
WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor *descriptor);
WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor *descriptor);
void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline);
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder);
WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor *descriptor);
void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer *commands);

/* Buffers (vertex/index/uniform). size/offset are bytes; the browser sandbox
   keeps these well under 2GB so they marshal as i32. */
WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor *descriptor);
void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void *data, size_t size);
void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size);
void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size);
void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference);

/* Bind groups + pipeline layout (uniform/storage buffer bindings; sampler &
   texture bindings land with the textures increment). */
WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor *descriptor);
WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor *descriptor);
WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor *descriptor);
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t *dynamicOffsets);

/* Textures + samplers. */
WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor *descriptor);
WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor *descriptor);
void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo *destination, const void *data, size_t dataSize, const WGPUTexelCopyBufferLayout *dataLayout, const WGPUExtent3D *writeSize);

/* GPU->CPU readback: render/copy to a buffer, map it async, read the bytes.
   getMappedRange returns a wasm-side copy of the mapped GPU bytes (read path);
   unmap frees it. copyTextureToBuffer requires bytesPerRow %256 == 0. */
void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo *source, const WGPUTexelCopyBufferInfo *destination, const WGPUExtent3D *copySize);
WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo);
void *wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size);
const void *wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size);
void wgpuBufferUnmap(WGPUBuffer buffer);

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);

/* Compute pipelines + passes. */
WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor *descriptor);
WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor *descriptor);
void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline);
void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t *dynamicOffsets);
void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder computePassEncoder, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ);
void wgpuComputePassEncoderEnd(WGPUComputePassEncoder computePassEncoder);

/* Error scopes: bracket GPU work to catch validation/oom errors in C (the
   message is also logged host-side). pop is async (callback model, NO JSPI). */
void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter);
WGPUFuture wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo);

/* Release/reference: free or retain a host handle. */
void wgpuBufferRelease(WGPUBuffer v);
void wgpuComputePipelineRelease(WGPUComputePipeline v);
void wgpuComputePassEncoderRelease(WGPUComputePassEncoder v);
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout v);
void wgpuBindGroupRelease(WGPUBindGroup v);
void wgpuPipelineLayoutRelease(WGPUPipelineLayout v);
void wgpuSamplerRelease(WGPUSampler v);
void wgpuInstanceRelease(WGPUInstance v);
void wgpuAdapterRelease(WGPUAdapter v);
void wgpuDeviceRelease(WGPUDevice v);
void wgpuQueueRelease(WGPUQueue v);
void wgpuSurfaceRelease(WGPUSurface v);
void wgpuTextureRelease(WGPUTexture v);
void wgpuTextureViewRelease(WGPUTextureView v);
void wgpuShaderModuleRelease(WGPUShaderModule v);
void wgpuRenderPipelineRelease(WGPURenderPipeline v);
void wgpuCommandEncoderRelease(WGPUCommandEncoder v);
void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder v);
void wgpuCommandBufferRelease(WGPUCommandBuffer v);

/* Frame loop (shared rAF; NO JSPI). Register a callback; it is called once per
   animation frame. Pass NULL to stop. Works with or without SDL. */
void wgpuSetMainLoopCallback(void (*callback)(void));

  