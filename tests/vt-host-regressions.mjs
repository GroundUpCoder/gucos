// Function-level fault injection; actual browser acceptance is separate.
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
let lose,submits=0;
const texture=()=>({createView:()=>({}),destroy(){}});
const pass={setPipeline(){},setBindGroup(){},draw(){},end(){}};
const device={lost:new Promise(r=>lose=r),addEventListener(){},destroy(){},
 queue:{writeTexture(){},submit(){submits++;}},createSampler:()=>({}),createShaderModule:()=>({}),
 createBindGroupLayout:()=>({}),createPipelineLayout:()=>({}),createRenderPipeline:()=>({getBindGroupLayout:()=>({})}),createTexture:texture,createBindGroup:()=>({}),
 createCommandEncoder:()=>({beginRenderPass:()=>pass,finish:()=>({})})};
const canvas={width:2,height:2,getContext:()=>({configure(){},unconfigure(){},getCurrentTexture:texture})};
const context=vm.createContext({console,TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,ArrayBuffer,Uint8Array,Uint32Array,Int32Array,Float32Array,DataView,Atomics,
 navigator:{gpu:{requestAdapter:async()=>({requestDevice:async()=>device}),getPreferredCanvasFormat:()=> 'bgra8unorm'}},GPUShaderStage:{FRAGMENT:2},GPUTextureUsage:{COPY_SRC:1,COPY_DST:2,TEXTURE_BINDING:4,RENDER_ATTACHMENT:16},WebAssembly:{...WebAssembly,Suspending:function(fn){return fn;}}});
vm.runInContext(await readFile(new URL('../src/host.js',import.meta.url),'utf8'),context);
const api=vm.runInContext('({createBrowserSDL,createBrowserWebGPU})',context);
const events=[];let adapter,dev;
const exports={__wgpu_call_adapter_cb:(cb,status,a)=>adapter=a,__wgpu_call_device_cb:(cb,status,d)=>dev=d};
const ctx={readString:()=>'',getMemory:()=>({buffer:new ArrayBuffer(64)}),getExports:()=>exports};
const sdl=api.createBrowserSDL({canvas,ctx,notifyWindow:e=>events.push(e)});
const handle=sdl.c.__sdl_create_window(0,0,0,2,2,8);assert(handle>0);
await sdl.gpuRendererReady();sdl.c.__sdl_update_window_surface(handle,0,2,2,8);
const renderer=sdl.c.__sdl_create_renderer(handle,0);assert(renderer>0);
assert(submits>0);const before=submits;lose({reason:'destroyed',message:'injected'});await Promise.resolve();
assert.throws(()=>sdl.c.__sdl_update_window_surface(handle,0,2,2,8),/device lost/);assert.equal(submits,before);
assert.throws(()=>sdl.c.__sdl_render_present(renderer),/device lost/);assert.equal(submits,before);
const gpu=api.createBrowserWebGPU({canvas,ctx,notifyWindow:e=>events.push(e),bindWindow:sdl.bindWindow});
const instance=gpu.c.__wgpu_create_instance();
gpu.c.__wgpu_instance_request_adapter(instance,1,0,0);await new Promise(r=>setTimeout(r,0));
gpu.c.__wgpu_adapter_request_device(adapter,1,0,0);await new Promise(r=>setTimeout(r,0));
assert.equal(gpu.c.__wgpu_instance_create_surface_for_window(instance,999),0);
const surface=gpu.c.__wgpu_instance_create_surface_for_window(instance,handle);assert(surface>0);
events.length=0;gpu.c.__wgpu_surface_configure(surface,dev,23,16,2,2,1,1,0,0);
assert.deepEqual(events.map(e=>e.type),['sdl-size']);
sdl.c.__sdl_destroy_window(handle);
assert.throws(()=>gpu.c.__wgpu_surface_configure(surface,dev,23,16,2,2,1,1,0,0),/owner window destroyed/);
gpu.close();sdl.close();
const listeners={},sent=[];
const inputCanvas={dataset:{relative:'true'},addEventListener:(name,fn)=>listeners[name]=fn,getBoundingClientRect:()=>({left:0,top:0,width:100,height:100}),width:100,height:100};
const inputContext=vm.createContext({document:{pointerLockElement:inputCanvas},navigator:{platform:'Mac'},console});
vm.runInContext(await readFile(new URL('../src/system-ui.js',import.meta.url),'utf8'),inputContext);
vm.runInContext('bindCanvasInput',inputContext)(inputCanvas,e=>sent.push(e));
listeners.pointermove({clientX:50,clientY:50,movementX:12,movementY:-7,buttons:0});
assert.equal(sent[0].type,'mouse-relative');assert.equal(sent[0].dx,12);assert.equal(sent[0].dy,-7);
console.log('PASS function probes: lost device blocks submissions, configure preserves visibility, invalid/stale SDL surface refused, relative movement retained');
