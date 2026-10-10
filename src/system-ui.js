// The canvas is letterboxed (object-fit: contain, centered): map through the
// displayed content box, not the element box.
function canvasPoint(rect,width,height,clientX,clientY){
  // The axis that fills the element keeps the direct mapping; the other
  // axis is centered with bars and shares the same scale.
  if(width*rect.height<=height*rect.width){
    const left=rect.left+(rect.width-width*rect.height/height)/2;
    return {x:(clientX-left)*height/rect.height,y:(clientY-rect.top)*height/rect.height};
  }
  const top=rect.top+(rect.height-height*rect.width/width)/2;
  return {x:(clientX-rect.left)*width/rect.width,y:(clientY-top)*width/rect.width};
}
function bindCanvasInput(canvas,send,textInput=false){
    const scans={Enter:40,Escape:41,Backspace:42,Tab:43,Space:44,ArrowRight:79,ArrowLeft:80,ArrowDown:81,ArrowUp:82,ControlLeft:224,ShiftLeft:225,AltLeft:226,MetaLeft:227,ControlRight:228,ShiftRight:229,AltRight:230,MetaRight:231,Delete:76,Home:74,End:77,PageUp:75,PageDown:78};
    // Physical USB/SDL key positions are required by XKB and SDL games even
    // when a separate Unicode keysym is available. Keep the named positions
    // consistent with BrowserInput in host.js (the standalone host path).
    Object.assign(scans,{Minus:45,Equal:46,BracketLeft:47,BracketRight:48,Backslash:49,
      Semicolon:51,Quote:52,Backquote:53,Comma:54,Period:55,Slash:56,CapsLock:57,
      PrintScreen:70,ScrollLock:71,Pause:72,Insert:73,NumLock:83,NumpadDivide:84,
      NumpadMultiply:85,NumpadSubtract:86,NumpadAdd:87,NumpadEnter:88,
      Numpad0:98,NumpadDecimal:99,IntlBackslash:100,ContextMenu:101,Digit0:39});
    for(let n=1;n<=9;n++){scans['Digit'+n]=29+n;scans['Numpad'+n]=88+n;}
    for(let n=1;n<=12;n++)scans['F'+n]=57+n;
    const held=new Map();
    let composing=false,paste=null;
    if(textInput){
    canvas.contentEditable='true';canvas.setAttribute('role','application');
    canvas.addEventListener('compositionstart',()=>{composing=true;});
    canvas.addEventListener('compositionend',e=>{composing=false;if(e.data)send({type:'text',text:e.data});});
    canvas.addEventListener('beforeinput',e=>{e.preventDefault();});
    paste=e=>{const text=e.clipboardData?.getData('text/plain');if(text!==undefined){e.preventDefault();send({type:'clipboard-paste',text});}};
    canvas.addEventListener('paste',paste);
    }
    const key=(e,down)=>{
      if(composing||e.isComposing||e.keyCode===229)return;
      const mac=/Mac/.test(globalThis.navigator?.platform||'');
      if(textInput&&(e.metaKey||(e.ctrlKey&&!mac))&&e.key.toLowerCase()==='v')return; // browser paste event carries text
      const appleEditing=textInput&&mac&&e.metaKey&&['a','c','x','z','s','o','f'].includes(e.key.toLowerCase());
      e.preventDefault();const scan=scans[e.code]||(/^Key[A-Z]$/.test(e.code)?e.code.charCodeAt(3)-61:0);
      const caps=!!e.getModifierState?.('CapsLock');
      // Virtual keyboards may supply an uppercase key without a separate
      // Shift event. Terminal editing retains its historical lowercase-plus-
      // modifiers encoding; SDL graphics receive modifier-applied Unicode.
      const shift=e.shiftKey||(down&&textInput&&/^[A-Z]$/.test(e.key)&&!caps);
      const character=textInput&&/^[A-Z]$/.test(e.key)?e.key.toLowerCase():e.key;
      const sym=[...e.key].length===1?character.codePointAt(0):({Enter:13,Escape:27,Backspace:8,Tab:9})[e.code]||(scan|0x40000000);
      const event={type:'key',down,scan,sym,repeat:e.repeat,mod:(shift?3:0)|((e.ctrlKey||appleEditing)?192:0)|(e.altKey?768:0)|((e.metaKey&&!appleEditing)?3072:0)|(caps?8192:0)};
      if(down)held.set(e.code,event);else held.delete(e.code);send(event);
    };
    canvas.addEventListener('keydown',e=>key(e,true));canvas.addEventListener('keyup',e=>key(e,false));
    canvas.addEventListener('blur',()=>{composing=false;for(const e of held.values())send({...e,down:false,repeat:false});held.clear();send({type:'focus',focused:false});});
    canvas.addEventListener('focus',()=>send({type:'focus',focused:true}));
    const point=e=>{
      const r=canvas.getBoundingClientRect();
      // Resizing a transferred OffscreenCanvas does not update the DOM
      // placeholder dimensions. SDL window messages carry the logical size.
      const width=Number(canvas.dataset?.logicalWidth)||canvas.width;
      const height=Number(canvas.dataset?.logicalHeight)||canvas.height;
      return canvasPoint(r,width,height,e.clientX,e.clientY);
    };
    canvas.addEventListener('pointerdown',e=>{
      e.preventDefault();canvas.focus();canvas.setPointerCapture(e.pointerId);
      send({type:'mouse-button',down:true,button:[1,2,3,4,5][e.button]||0,...point(e)});
    });
    canvas.addEventListener('pointerup',e=>{
      send({type:'mouse-button',down:false,button:[1,2,3,4,5][e.button]||0,...point(e)});
      if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
    });
    for(const type of ['pointerenter','pointerleave'])
      canvas.addEventListener(type,e=>send({type:'mouse-crossing',entered:type==='pointerenter',...point(e)}));
    canvas.addEventListener('pointermove',e=>send({type:'mouse-motion',state:(e.buttons&1)|((e.buttons&2)<<1)|((e.buttons&4)>>1)|(e.buttons&24),...point(e)}));
    canvas.addEventListener('contextmenu',e=>e.preventDefault());
    canvas.addEventListener('wheel',e=>{e.preventDefault();const p=point(e);send({type:'mouse-wheel',x:-Math.sign(e.deltaX),y:-Math.sign(e.deltaY),pointerX:p.x,pointerY:p.y});},{passive:false});
    return paste;
}
class BusyBoxSession {
  constructor(payload, ui, terminal) {
    Object.assign(this,{payload,ui,terminal});this.windows=new Map();this.surfaceWindows=new Map();this.log='';this.ready=false;this.requests=new Map();this.nextId=1;this.compilations=new Map();
    // Chromium can target BODY for native paste even while a canvas has focus.
    // Route that event to the focused surface; leave terminal/DOM inputs alone.
    this.nativePaste=event=>{
      if(event.defaultPrevented)return;
      for(const record of this.surfaceWindows.values())if(record.canvas===document.activeElement&&event.target!==record.canvas){record.paste?.(event);break;}
    };
    document.addEventListener('paste',this.nativePaste);
  }
  async start(args=[]) {
    if(!crossOriginIsolated||typeof SharedArrayBuffer==='undefined')throw new Error('BusyBox requires the local HTTP server with COOP/COEP headers. Run npm run serve and open localhost:8080; file:// retains the bootstrap shell.');
    if(this.worker){
      if(this.running)throw new Error('A foreground command is already running');
      this.running=true;this.ui.session=this;this.terminal.write('\x1b[?2004l');
      this.done=new Promise((resolve,reject)=>{this.resolve=resolve;this.reject=reject;});
      this.worker.postMessage({type:'run',args});return this.done;
    }
    this.running=true;
    const makeURL=source=>URL.createObjectURL(new Blob([source],{type:'text/javascript'}));
    this.compilerURL=this.payload.systemCompilerURL?new URL(this.payload.systemCompilerURL,location.href).href:makeURL(this.payload.systemCompiler);this.processURL=this.payload.systemProcessURL?new URL(this.payload.systemProcessURL,location.href).href:makeURL(this.payload.systemProcess);this.ownerURL=this.payload.systemOwnerURL?new URL(this.payload.systemOwnerURL,location.href).href:makeURL(this.payload.systemOwner);
    this.worker=new Worker(this.ownerURL,{name:'busybox-filesystem-owner'});
    this.done=new Promise((resolve,reject)=>{this.resolve=resolve;this.reject=reject;});
    this.worker.onmessage=({data})=>{
      if(data.type==='output'){this.log+=data.text;this.ui.print(data.text);}
      if(data.type==='ready'){if(this.payload.hosted){for(const path of Object.keys(edited))delete edited[path];try{localStorage.removeItem(storageNames.edits);}catch{}}this.ready=true;this.persistence=data.persistence;$('status').textContent='BusyBox · '+data.persistence;}
      if(data.type==='compilation'){if(data.active)this.compilations.set(data.id,data);else this.compilations.delete(data.id);}
      if(data.type==='built'){this.ui.print(`${data.name}: ${data.stats.functions} functions, ${data.stats.bytes} bytes\n`);}
      if(data.type==='clipboard')navigator.clipboard?.writeText(data.text).catch(()=>{});
      if(data.type==='audio-output'){this.audioPlayer?.close();this.audioPlayer=createSystemAudioPlayer(data);}
      if(data.type==='surface')this.updateSurface(data);
      if(data.type==='surface-gone'){this.surfaceWindows.get(data.sid)?.win.destroy();this.surfaceWindows.delete(data.sid);this.updateDesktopSession();}
      if(data.type==='process')this.createWindow(data.pid,data.path);
      if(data.type==='window'){
        const win=this.windows.get(data.pid), event=data.event;
        if(win){
          if(event.handle)win.handle=event.handle;
          if((event.type==='sdl-window'||event.type==='sdl-size')&&event.width>0&&event.height>0){
            win.canvas.dataset.logicalWidth=event.width;win.canvas.dataset.logicalHeight=event.height;
          }
          if(event.type==='sdl-visible'||event.type==='sdl-destroy'){
            win.programHidden=event.type==='sdl-destroy'||event.hidden;
            win.element.hidden=win.programHidden;win.task.hidden=win.programHidden;
            if(!win.programHidden)win.focus();
          }else if(event.type==='sdl-raise')win.focus();
          else if(event.type==='sdl-size'){
            win.geometry.w=event.width+2;win.geometry.h=event.height+36;win.apply();
          }else{
            if(event.type==='sdl-window'){win.shown=false;win.minimized=false;win.programHidden=!!event.hidden;}
            win.show({...event,...(event.title?{title:`[${data.pid}] ${event.title}`}:{})});
            if(event.hidden!==undefined)win.programHidden=event.hidden;
            if(win.programHidden){win.element.hidden=true;win.task.hidden=true;}
          }
        }
      }
      if(data.type==='process-exit'){this.windows.get(data.pid)?.destroy();this.windows.delete(data.pid);for(const [sid,record]of this.surfaceWindows)if(record.meta.pid===data.pid){record.win.destroy();this.surfaceWindows.delete(sid);}}
      if(data.type==='file'||data.type==='synced'||data.type==='written'||data.type==='metadata'){this.requests.get(data.id)?.(data.value??data.bytes);this.requests.delete(data.id);}
      if(data.type==='command-exit'){
        this.running=false;this.ui.session=null;this.terminal.write('\x1b[?2004h');
        $('status').textContent='Kernel idle · '+this.persistence;
        this.resolve({code:data.status>>>8,diskBytes:data.diskBytes});
      }
      if(data.type==='halt'){
        for(const path of data.deleted||[]){delete files[path];delete edited[path];}
        if(data.files){for(const [path,text] of Object.entries(data.files)){files[path]=edited[path]=text;}try{localStorage.setItem(storageNames.edits,JSON.stringify(edited));}catch{}}
        this.close();this.resolve({code:data.status>>>8,diskBytes:data.diskBytes});
      }
      if(data.type==='error'){this.close();this.reject(new Error(data.text));}
    };
    this.worker.onerror=error=>{this.close();this.reject(new Error(error.message));};
    this.ui.session=this;
    this.terminal.write('\x1b[?2004l');
    this.resizeSubscription=this.terminal.onResize(({cols,rows})=>this.worker.postMessage({type:'terminal-size',cols,rows}));
    this.screenSize=()=>this.worker?.postMessage({type:'screen-size',width:innerWidth,height:innerHeight-32});
    window.addEventListener('resize',this.screenSize);
    // Binary fixtures are disk data; keep them out of the text editor's source map.
    // Explicit edits take precedence over bundled defaults.
    const bootFiles={...Object.fromEntries(Object.entries(this.payload.binaryFiles||{}).map(([path,b64])=>[path,unbase64(b64)])),...files};
    // Boot executables compiled by the bundler (tools/prebuilt.mjs).
    const prebuilt=Object.fromEntries(Object.entries(this.payload.prebuilt||{}).map(([path,e])=>[path,e.url?{key:e.key,url:new URL(e.url,location.href).href}:{bytes:unbase64(e.bytes),key:e.key}]));
    // Prebuilt package artifacts live next to the page when the server
    // publishes them (docs/PACKAGES.md); gucman fetches by key from here.
    const artifacts=/^https?:$/.test(location.protocol)?new URL('artifacts/',location.href).href:'';
    this.worker.postMessage({type:'boot',screenWidth:innerWidth,screenHeight:innerHeight-32,dataFiles:Object.fromEntries(Object.entries(this.payload.dataFiles||{}).map(([path,url])=>[path,new URL(url,location.href).href])),hosted:!!this.payload.hosted,packageLicenses:Object.fromEntries(Object.entries(this.payload.packageLicenses||{}).map(([name,url])=>[name,new URL(url,location.href).href])),sourceArchives:Object.fromEntries(Object.entries(this.payload.sourceArchives||{}).map(([tree,url])=>[tree,new URL(url,location.href).href])),processURL:this.processURL,compilerURL:this.compilerURL,files:bootFiles,edited,args,prebuilt,artifacts,repository:this.payload.repository,repositoryScripts:this.payload.repositoryScripts,busyboxForkMigration:this.payload.busyboxForkMigration,sharedPackageMigration:this.payload.sharedPackageMigration,textPackageMigration:this.payload.textPackageMigration,buildId:this.payload.buildId,packageKeys:this.payload.packageKeys,toolchain:this.payload.toolchain,versions:this.payload.versions,
      shapingPackageMigration:this.payload.shapingPackageMigration,
      gtkPackageMigration:this.payload.gtkPackageMigration,
      gioPackageMigration:this.payload.gioPackageMigration,
      applets:this.payload.applets,executables:Object.fromEntries([...executables].map(([path,e])=>[path,e.bytes])),cols:this.terminal.cols,rows:this.terminal.rows});
    return this.done;
  }
  createWindow(pid,path) {
    const canvas=document.createElement('canvas');canvas.width=640;canvas.height=400;canvas.tabIndex=0;
    const send=event=>this.worker.postMessage({type:'process-input',pid,event:{handle:win.handle||1,...event}});
    const win=new DesktopWindow(canvas,()=>this.worker.postMessage({type:'kill',pid}),(width,height)=>send({type:'resize',width,height}),()=>this.terminal.focus());
    win.title.textContent=`[${pid}] ${path}`;this.windows.set(pid,win);
    bindCanvasInput(canvas,send);
    const offscreen=canvas.transferControlToOffscreen();this.worker.postMessage({type:'canvas',pid,canvas:offscreen},[offscreen]);
  }
  updateDesktopSession(){
    const active=[...this.surfaceWindows.values()].some(r=>r.meta.layer===-1&&r.meta.borderless&&r.meta.viewable&&r.painted);
    const top=active?Math.max(0,...[...this.surfaceWindows.values()].filter(r=>r.meta.viewable&&r.meta.borderless&&r.meta.layer===1&&r.meta.y===0&&r.meta.dstW>=innerWidth-4).map(r=>r.meta.dstH)):0;
    for(const {win} of this.surfaceWindows.values())if(win.onViewportResize&&(win.workAreaTop||0)!==top){win.workAreaTop=top;win.onViewportResize();}
    if(active!==!!this.desktopSession){this.desktopSession=active;window.dispatchEvent(new CustomEvent('desktop-session',{detail:{active}}));}
  }
  updateSurface({meta,frame}) {
    let record=this.surfaceWindows.get(meta.sid);
    if(!record){
      const canvas=document.createElement('canvas');canvas.tabIndex=0;
      const send=event=>{if(event.type==='clipboard-paste'){this.worker.postMessage({type:'clipboard',text:event.text});for(const down of [true,false])this.worker.postMessage({type:'surface-input',sid:meta.sid,event:{type:'key',down,scan:25,sym:118,mod:192,repeat:false}});return;}this.worker.postMessage({type:'surface-input',sid:meta.sid,event:{serial:record.meta.committedSerial,...event}});};
      let win;
      if(meta.borderless||meta.parent){
        const element=document.createElement('section');element.className='screen';element.hidden=true;
        Object.assign(element.style,{border:'0',borderRadius:'0',minWidth:'1px',minHeight:'1px',boxShadow:'none',background:'transparent'});
        element.append(canvas);document.getElementById('screens').append(element);
        win={element,canvas,destroy:()=>element.remove()};
      }else{
        win=new DesktopWindow(canvas,()=>{send({type:'close'});return false;},(width,height)=>send({type:'resize',width,height}),()=>this.terminal.focus());
        win.kernelGeometry=true;
        win.onActivate=()=>send({type:'activate'});
        win.onMinimize=()=>send({type:'minimize'});
        win.onMove=(x,y)=>send({type:'move',x:x+1,y:y+35});
        win.task.onclick=()=>send({type:'activate'});
        win.show({title:meta.title,width:meta.dstW,height:meta.dstH,resizable:meta.resizable,hidden:true});
      }
      record={win,canvas,context:canvas.getContext('2d'),meta:{}};this.surfaceWindows.set(meta.sid,record);
      record.paste=bindCanvasInput(canvas,send,true);
    }
    // Fit newly shown desktop windows into the work area. GTK file dialogs
    // can request a natural size larger than the viewport. Ask the application
    // to resize once, retaining kernel geometry and input-coordinate ownership.
    if(meta.viewable&&!record.wasViewable&&!meta.borderless&&!meta.parent){
      const width=Math.max(160,Math.min(meta.dstW,innerWidth-16));
      const height=Math.max(100,Math.min(meta.dstH,innerHeight-84-(record.win.workAreaTop||0)));
      const x=Math.max(8,Math.min(meta.x,innerWidth-width-8));
      const y=Math.max(43+(record.win.workAreaTop||0),Math.min(meta.y,innerHeight-40-height));
      if(meta.resizable&&(width!==meta.dstW||height!==meta.dstH))this.worker.postMessage({type:'surface-input',sid:meta.sid,event:{type:'resize',width,height}});
      if(x!==meta.x||y!==meta.y)this.worker.postMessage({type:'surface-input',sid:meta.sid,event:{type:'move',x,y}});
    }
    record.wasViewable=meta.viewable;
    const {win,canvas}=record,previous=record.meta;record.meta=meta;
    win.element.dataset.surfaceId=meta.sid;
    win.element.hidden=!meta.viewable;
    win.element.style.zIndex=String(100+meta.z);
    win.element.classList.toggle('active',meta.focused);
    if(win.task){
      win.task.hidden=!meta.requestedVisible||!!meta.owner;win.title.textContent=meta.title;win.task.textContent=meta.title;
      if(previous.x!==meta.x||previous.y!==meta.y||previous.dstW!==meta.dstW||previous.dstH!==meta.dstH){
        win.geometry={x:meta.x-1,y:meta.y-35,w:meta.dstW+2,h:meta.dstH+36};win.apply();
      }
    }else Object.assign(win.element.style,{left:meta.x+'px',top:meta.y+'px',width:meta.dstW+'px',height:meta.dstH+'px'});
    if(frame){
      record.painted=true;
      if(canvas.width!==frame.w)canvas.width=frame.w;if(canvas.height!==frame.h)canvas.height=frame.h;
      record.context.putImageData(new ImageData(new Uint8ClampedArray(frame.rgba.buffer,frame.rgba.byteOffset,frame.rgba.byteLength),frame.w,frame.h),0,0);
    }
    if(meta.focused&&!previous.focused&&meta.viewable)canvas.focus();
    this.updateDesktopSession();
  }
  metadata(path){return new Promise(resolve=>{const id=this.nextId++;this.requests.set(id,resolve);this.worker.postMessage({type:'metadata',path,id});});}
  read(path){return new Promise(resolve=>{const id=this.nextId++;this.requests.set(id,resolve);this.worker.postMessage({type:'read-file',path,id});});}
  // Host harnesses seed disk files (cached package artifacts) into a running session.
  write(path,bytes){return new Promise(resolve=>{const id=this.nextId++;this.requests.set(id,resolve);this.worker.postMessage({type:'write-file',path,bytes,id});});}
  close(){
    this.audioPlayer?.close();this.audioPlayer=null;
    document.removeEventListener('paste',this.nativePaste);
    window.removeEventListener('resize',this.screenSize);this.resizeSubscription?.dispose();for(const win of this.windows.values())win.destroy();this.windows.clear();for(const record of this.surfaceWindows.values())record.win.destroy();this.surfaceWindows.clear();
    this.worker?.terminate();this.worker=null;this.running=false;URL.revokeObjectURL(this.processURL);URL.revokeObjectURL(this.ownerURL);URL.revokeObjectURL(this.compilerURL);this.compilations.clear();
    if(this.ui.session===this)this.ui.session=null;this.terminal.write('\x1b[?2004h');
    $('status').textContent='Bootstrap shell';
  }
}
