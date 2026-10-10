function canvasCursor(canvas,shape){canvas.style.cursor=['default','text','wait','crosshair','progress','nwse-resize','nesw-resize','ew-resize','ns-resize','move','not-allowed','pointer'][shape]||(shape===-1?'none':'default');}
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
    const held=new Map(),buttons=new Map(),pointers=new Set();
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
    canvas.addEventListener('blur',()=>{composing=false;for(const e of held.values())send({...e,down:false,repeat:false});held.clear();for(const id of pointers)if(canvas.hasPointerCapture(id))canvas.releasePointerCapture(id);pointers.clear();for(const e of buttons.values())send({...e,down:false});buttons.clear();send({type:'focus',focused:false});});
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
      e.preventDefault();canvas.focus();canvas.setPointerCapture(e.pointerId);pointers.add(e.pointerId);
      const event={type:'mouse-button',down:true,button:[1,2,3,4,5][e.button]||0,...point(e)};buttons.set(e.button,event);send(event);
      if(canvas.dataset.relative==='true')canvas.requestPointerLock()?.catch(error=>console.error(error));
    });
    canvas.addEventListener('pointerup',e=>{
      pointers.delete(e.pointerId);buttons.delete(e.button);send({type:'mouse-button',down:false,button:[1,2,3,4,5][e.button]||0,...point(e)});
      if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId);
    });
    for(const type of ['pointerenter','pointerleave'])
      canvas.addEventListener(type,e=>send({type:'mouse-crossing',entered:type==='pointerenter',...point(e)}));
    canvas.addEventListener('pointermove',e=>send({type:document.pointerLockElement===canvas?'mouse-relative':'mouse-motion',dx:e.movementX,dy:e.movementY,state:(e.buttons&1)|((e.buttons&2)<<1)|((e.buttons&4)>>1)|(e.buttons&24),...point(e)}));
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
      if(data.type==='process')this.createWindow(data.pid,data.path);
      if(data.type==='window'){
        const win=this.windows.get(data.pid), event=data.event;
        if(win){
          if(event.handle)win.handle=event.handle;
          if(event.type==='sdl-cursor'){canvasCursor(win.canvas,event.shape);return;}
          if(event.type==='sdl-relative-mouse'){win.canvas.dataset.relative=String(event.enabled);if(!event.enabled&&document.pointerLockElement===win.canvas)document.exitPointerLock();return;}
          if(event.type==='present')return;
          if((event.type==='sdl-window'||event.type==='sdl-size')&&event.width>0&&event.height>0){
            win.canvas.dataset.logicalWidth=event.width;win.canvas.dataset.logicalHeight=event.height;
          }
          if(event.type==='sdl-visible'||event.type==='sdl-destroy'){
            win.programHidden=event.type==='sdl-destroy'||event.hidden;
            win.setVisible(!win.programHidden);
            if(!win.programHidden)win.focus();
          }else if(event.type==='sdl-raise')win.focus();
          else if(event.type==='sdl-size'){
            win.geometry.w=event.width+2;win.geometry.h=event.height+36;win.apply();
          }else{
            if(event.type==='sdl-window'){win.shown=false;win.minimized=false;win.programHidden=!!event.hidden;}
            win.show({...event,...(event.title?{title:`[${data.pid}] ${event.title}`}:{})});
            if(event.hidden!==undefined)win.programHidden=event.hidden;
            if(win.programHidden)win.setVisible(false);
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
    const win=new VirtualTerminal(canvas,()=>this.worker.postMessage({type:'kill',pid}),(width,height)=>send({type:'resize',width,height}),()=>this.terminal.focus());
    win.title.textContent=`[${pid}] ${path}`;this.windows.set(pid,win);
    win.visibility=active=>send({type:'vt-active',active});
    bindCanvasInput(canvas,send);
    const offscreen=canvas.transferControlToOffscreen();this.worker.postMessage({type:'canvas',pid,canvas:offscreen},[offscreen]);
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
