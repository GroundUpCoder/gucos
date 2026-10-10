self.vtActive=false;
// Process bootstrap for the shared BusyBox filesystem/TTY broker.
let sdl, instance, canvasResolve;
let controlId=0;
const controls=new Map();
function control(op,data){return new Promise(resolve=>{const id=++controlId;controls.set(id,resolve);postMessage({type:'process-control',id,op,data},op==='fork'?[data.snapshot.memory]:[]);});}
let canvasReady=new Promise(resolve=>canvasResolve=resolve);
self.onmessage=async({data})=>{
  if(data.type==='process-result'){controls.get(data.id)?.(data.result);controls.delete(data.id);return;}
  if(data.type==='canvas'){canvasResolve(data.canvas);return;}
  if(data.type==='vt-active'){self.vtActive=!!data.active;return;}
  if(data.type==='key'){sdl?.pushKeyEvent(data.handle||1,data.down?0x300:0x301,data.scan,data.sym,data.mod,data.repeat);return;}
  if(data.type==='mouse-button'){sdl?.pushMouseButtonEvent(data.handle||1,data.down?0x401:0x402,data.button,data.x,data.y);return;}
  if(data.type==='mouse-crossing'){sdl?.pushWindowEvent(data.handle||1,data.entered?0x20c:0x20d,data.x|0,data.y|0);return;}
  if(data.type==='mouse-relative'){sdl?.pushMouseMotionRelEvent(data.handle||1,data.dx,data.dy,data.state);return;}
  if(data.type==='mouse-motion'){sdl?.pushMouseMotionEvent(data.handle||1,data.x,data.y,data.state);return;}
  if(data.type==='mouse-wheel'){sdl?.pushMouseWheelEvent(data.handle||1,data.x,data.y,0);return;}
  if(data.type==='focus'){sdl?.pushWindowEvent(data.handle||1,data.focused?0x20e:0x20f,0,0);return;}
  if(data.type==='resize'){
    sdl?.pushWindowEvent(data.handle||1,0x206,data.width,data.height);
    return;
  }
  if(data.type!=='start')return;
  let wd=data.spec;
  const client=new KERNEL.KernelClient(wd.kernelPage,(msg,transfers)=>postMessage(msg,transfers));
  let rfs=new KERNEL.RemoteFS(client,{fileReadCache:wd.fileReadCache});
  if(wd.forkSnapshot?.fsState){rfs._fdTable=wd.forkSnapshot.fsState.fds;rfs._dirs=wd.forkSnapshot.fsState.dirs;}
  for(const p of wd.pipeRings||[])rfs.registerPipeRing(p.fd,p.end,p.sab);
  let env={};for(const entry of wd.envp||[]){const i=entry.indexOf('=');if(i>0)env[entry.slice(0,i)]=entry.slice(i+1);}
  // A thread worker has no canvas of its own; it shares the process memory.
  let canvas=wd.thread?null:await canvasReady;self.processCanvas=canvas;
  for(;;)try {
    const code=await runModule({
      bytes:wd.image||undefined,module:wd.module||undefined,args:wd.argv,executablePath:wd.path,env,pid:wd.pid,ppid:wd.ppid,thread:wd.thread||undefined,vm:wd.vm||undefined,
      getppid:()=>client.getppid(),stdinSab:wd.ttySab||undefined,getBrowserSDL:canvas,
      blockFsFactory:async ctx=>{
        const imports=BLOCK_FS.BlockFS.prototype.toWasmEnv.call(rfs,ctx);
        imports.__select_impl=rfs.selectImpl(ctx,true);imports.__poll_impl=rfs.pollImpl(ctx,true);imports.isatty=fd=>rfs.isatty(fd);
        // Mapped memory and record locks are kernel requests (docs/MAPPINGS.md).
        const call=(op,a,wait)=>{const r=client.call(op,a,wait);if(r.errno){ctx.setErrnoName(r.errno);return null;}return r;};
        imports.__mmap=(addr,length,prot,flags,fd,offset)=>{const r=call(KERNEL.OP.FS_MMAP,{length:length>>>0,prot,flags,fd,offset:Number(offset)});return r?r.address|0:0;};
        imports.__munmap=(address,length)=>call(KERNEL.OP.FS_MUNMAP,{address:address>>>0,length:length>>>0})?0:-1;
        imports.__mprotect=(address,length,prot)=>call(KERNEL.OP.FS_MPROTECT,{address:address>>>0,length:length>>>0,prot})?0:-1;
        imports.__msync=(address,length,flags)=>call(KERNEL.OP.FS_MSYNC,{address:address>>>0,length:length>>>0,flags})?0:-1;
        imports.__fcntl_lock=(fd,cmd,ptr)=>{
          const dv=new DataView(ctx.getMemory().buffer);
          const r=call(KERNEL.OP.FS_FCNTL_LOCK,{fd,cmd,type:dv.getInt16(ptr,true),whence:dv.getInt16(ptr+2,true),start:Number(dv.getBigInt64(ptr+8,true)),len:Number(dv.getBigInt64(ptr+16,true))},cmd===7);
          if(!r)return -1;
          if(cmd===5){const v=new DataView(ctx.getMemory().buffer);v.setInt16(ptr,r.type,true);if(r.type!==2){v.setInt16(ptr+2,0,true);v.setBigInt64(ptr+8,BigInt(r.start),true);v.setBigInt64(ptr+16,BigInt(r.len),true);v.setInt32(ptr+24,r.pid,true);}}
          return 0;
        };
        return {c:imports};
      },
      dynamicReadFile:(path,requester,runpath=[])=>{
        const resolved=requester?rfs.realpathPhysical(requester):null;
        const origin=resolved?resolved.slice(0,resolved.lastIndexOf('/')):(rfs.getcwd()||'.');
        const candidates=path.includes('/')?[path]:[...(env.LD_LIBRARY_PATH||'').split(':').filter(Boolean).map(p=>p+'/'+path),...runpath.map(p=>p.replace(/\$\{ORIGIN\}|\$ORIGIN/g,origin)+'/'+path),'/usr/local/lib/'+path,'/usr/lib/'+path];
        return control('load-library',{candidates}).then(result=>{
          if(result.error||result.errno)throw new Error(result.error||result.errno);
          return result.library||null;
        });
      },
      spawnHooks:rfs.wrapSpawnHooks(client.spawnHooks()),
      forkSnapshot:wd.forkSnapshot,sharedChild:wd.sharedChild,execState:wd.execState,
      processControl:(op,data)=>{
        if(op==='fork')data.snapshot.fsState={fds:rfs._fdTable,dirs:rfs._dirs};
        return control(op,data);
      },
      writeOut:bytes=>postMessage({type:'out',fd:1,bytes:typeof bytes==='string'?new TextEncoder().encode(bytes):bytes.slice()}),
      writeErr:bytes=>postMessage({type:'out',fd:2,bytes:typeof bytes==='string'?new TextEncoder().encode(bytes):bytes.slice()}),
      onReady:ready=>{sdl=ready.sdl;instance=ready.instance;if(wd.forkSnapshot)postMessage({type:'fork-ready'});},
      notifyWindow:event=>postMessage({type:'window',event})
    });
    postMessage({type:wd.thread?'thread-exited':'exited',code});break;
  } catch(error){
    if(error.execReplacement){
      const next=error.execReplacement;
      if(wd.sharedChild)await control('shared-exec-ready',{});
      wd={...wd,module:next.module,image:null,argv:next.argv,envp:next.envp,path:next.path,forkSnapshot:null,sharedChild:null,execState:next,vm:next.vm||null};
      env={};for(const entry of wd.envp){const i=entry.indexOf('=');if(i>0)env[entry.slice(0,i)]=entry.slice(i+1);}
      rfs=new KERNEL.RemoteFS(client,{fileReadCache:wd.fileReadCache});
      for(const p of next.pipeRings)rfs.registerPipeRing(p.fd,p.end,p.sab);
      canvasReady=new Promise(resolve=>canvasResolve=resolve);sdl=null;instance=null;
      postMessage({type:'image-replaced',path:wd.path});
      canvas=await canvasReady;self.processCanvas=canvas;
      continue;
    }
    postMessage({type:'crashed',error:error.stack||String(error)});break;
  }
};
