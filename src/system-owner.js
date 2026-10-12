// One filesystem owner. OPFS handles and all fd/pipe state stay in this worker.
let systemFS, kernel, tty, processURL, compilerJobs, closeDisk=()=>{}, commitSoon=()=>{}, diskSize=()=>0;
const workerHandles=new Map();
let commandPid=0;
const terminalSessions=new Map(), execSessions=new Map();
const shellEnvironment=()=>['PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin','HOME='+HOME_DIRECTORY,'USER=user','LOGNAME=user','PWD='+HOME_DIRECTORY,'TERM=xterm-256color','PS1=\\w $ '];
function stopTty(terminal){for(const p of kernel._procs.values())if((p.launchTty||p.tty)===terminal)kernel.kill(p.pid,9,0);}

let bootDownloads=new Map();
const bootStatus=phase=>postMessage({type:'boot-progress',phase});
function needsCoreLibc(data){
  if(!systemFS.stat('/usr/lib/libc.a')||!systemFS.stat('/usr/lib/libc.so.1')||!systemFS.stat('/usr/lib/libc-fork.so.1'))return true;
  try{return JSON.parse(decoder.decode(readFile('/usr/lib/wasm-posix/libc.json'))).libcDigest!==data.toolchain.libcDigest;}catch{return true;}
}
function planBootDownloads(data){
  const names={'/usr/lib/libc.a':'C development library','/usr/lib/libc.so.1':'C runtime','/usr/lib/libc-fork.so.1':'Process runtime','/usr/lib/wasm-posix/libc.json':'Runtime information','/bin/ash':'Shell','/bin/busybox':'System utilities','/usr/bin/make':'Make build tool','/bin/cc':'C compiler launcher','/usr/bin/ar':'Archive tool','/usr/bin/guc-fetch':'Download tool','/usr/bin/guc-links':'Link tool','/usr/libexec/guc-lock':'Package lock helper','/usr/libexec/guc-json':'Package metadata helper','/usr/libexec/guc-repo-fetch':'Package download helper','/usr/libexec/guc-unpack':'Package unpack helper','/usr/libexec/guc-links-v2':'Package link helper'};
  const add=(path,entry)=>{if(entry?.url&&!bootDownloads.has(entry.url))bootDownloads.set(entry.url,{id:entry.url,name:names[path]||path.split('/').pop(),total:entry.size});};
  bootDownloads=new Map();
  for(const [path,url] of Object.entries(data.dataFiles||{}))if(!systemFS.lstat(path))add(path,{url});
  if(needsCoreLibc(data))for(const path of ['/usr/lib/libc.a','/usr/lib/libc.so.1','/usr/lib/libc-fork.so.1','/usr/lib/wasm-posix/libc.json'])add(path,data.prebuilt?.[path]);
  for(const path of ['/bin/ash','/bin/busybox','/usr/bin/make','/bin/cc','/usr/bin/ar','/usr/bin/guc-fetch','/usr/bin/guc-links'])if(!systemFS.stat(path))add(path,data.prebuilt?.[path]);
  for(const path of (data.repository?['/usr/libexec/guc-lock','/usr/libexec/guc-json','/usr/libexec/guc-repo-fetch','/usr/libexec/guc-unpack','/usr/libexec/guc-links-v2']:['/usr/bin/guc-links']))add(path,data.prebuilt?.[path]);
  postMessage({type:'boot-progress',phase:'Preparing your OS',items:[...bootDownloads.values()]});
}
async function rawAsset(url){
  const item=bootDownloads.get(url), report=(state,loaded)=>{if(item&&item.state!=='done'){Object.assign(item,{state,loaded});postMessage({type:'boot-progress',item:{...item}});}};
  try{
    report('downloading',0);
    const response=await fetch(url);if(!response.ok)throw new Error(`Download failed (${response.status}): ${item?.name||url}`);
    const reader=response.body.getReader(),chunks=[];let length=0;
    while(true){const {done,value}=await reader.read();if(done)break;chunks.push(value);length+=value.length;report('downloading',length);}
    const bytes=new Uint8Array(length);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
    report('verifying',length);
    const hash=[...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(b=>b.toString(16).padStart(2,'0')).join('');
    if(!new URL(url).pathname.endsWith('/'+hash+'.bin'))throw new Error('Download verification failed: '+(item?.name||url));
    report('done',length);
    return bytes;
  }catch(error){report('failed',item?.loaded||0);throw error;}
}
async function runCommand(args){
  if(commandPid)throw new Error('A foreground command is already running');
  tty.interactiveOut=!args.length;tty.fgPgid=0;
  commandPid=await kernel.boot({path:'/bin/sh',argv:['/bin/sh',...args],cwd:HOME_DIRECTORY,envp:['PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin','HOME='+HOME_DIRECTORY,'USER=user','LOGNAME=user','PWD='+HOME_DIRECTORY,'TERM=xterm-256color','PS1=\\w $ ']});
}
const decoder=new TextDecoder(), encoder=new TextEncoder();
function output(text){postMessage({type:'output',text});}
let audioTimer=null,audioOutput=null;
function scheduleAudio(){
  if(!audioOutput){audioOutput=kernel.audioInit();postMessage({type:'audio-output',...audioOutput});}
  if(audioTimer)return;
  audioTimer=setInterval(()=>{
    kernel.audioPump();
    if(!kernel.audioStreamCount()){clearInterval(audioTimer);audioTimer=null;}
  },10);
}
async function initialize(data){
  bootStatus('Opening your disk…');
  processURL=data.processURL;
  compilerJobs=new CompilerJobs(data.compilerURL,{
    readFile,snapshot,
    writeFile:(...args)=>{writeFile(...args);commitSoon();},
    stat:path=>systemFS.stat(path),
    chmod:(path,mode)=>{const result=systemFS.chmod(path,mode);commitSoon();return result;},
    flush:()=>systemFS._s.flush()
  },{buildId:data.buildId,toolchain:data.toolchain,output,onState:state=>postMessage({type:'compilation',...state})});
  const storage=installationStorage(data.repository);
  let persistence='OPFS';
  try{
    const root=await navigator.storage.getDirectory();
    const file=await root.getFileHandle(storage.disk,{create:true});
    const handle=await file.createSyncAccessHandle();
    // The redo journal makes every commit atomic (docs/STORAGE.md); the
    // kernel commits after each request that changed the disk.
    const journalHandle=await (await root.getFileHandle(storage.journal,{create:true})).createSyncAccessHandle();
    const store=new JournalStore(handle,journalHandle);
    if(store.size()<(1<<20))store.resize(1<<20);
    systemFS=BLOCK_FS.create(store);
    // Requests that changed the disk are committed within COMMIT_MS; fsync
    // commits at once (BlockFS calls store.flush). A checkpoint every second
    // moves committed pages into the image and empties the journal.
    const COMMIT_MS=20;let commitTimer=null;
    const report=error=>postMessage({type:'error',text:'disk: '+error.message});
    commitSoon=()=>{if(commitTimer!==null)return;commitTimer=setTimeout(()=>{commitTimer=null;try{store.commit();}catch(error){report(error);}},COMMIT_MS);};
    const timer=setInterval(()=>{try{store.checkpoint();}catch(error){report(error);}},1000);
    diskSize=()=>store.size();
    closeDisk=()=>{clearInterval(timer);if(commitTimer!==null)clearTimeout(commitTimer);store.checkpoint();handle.close();journalHandle.close();closeDisk=()=>{};};
  }
  catch(error){if(error.name==='NoModificationAllowedError')throw new Error('The BusyBox disk is open in another tab. Close that session first.');if(!['SecurityError','NotSupportedError'].includes(error.name))throw error;persistence='memory';systemFS=BLOCK_FS.create(new BLOCK_FS.MemoryByteStore(1<<20));output(`OPFS unavailable (${error.message}); this session uses memory.\n`);}
  for(const dir of SYSTEM_DIRECTORIES)systemFS.mkdir(dir,dir==='/tmp'||dir==='/var/tmp'?0o1777:0o755);
  // The v3 formatter does not self-heal /dev; shells open /dev/null for background stdin.
  systemFS.ensureDevNodes();
  // Upgrade the existing disk in place; never overwrite a user's destination.
  if(!systemFS.stat('/etc/.wasm-posix-layout-v2')){
    const move=(from,to)=>{if(systemFS.lstat(from)&&!systemFS.lstat(to)){mkdirs(to);if(systemFS.rename(from,to)!==0)throw new Error(`Cannot migrate ${from}`);}};
    for(const [from,to] of (typeof DISTRIBUTION_LAYOUT==='undefined'?[]:DISTRIBUTION_LAYOUT.moves))move(from,to);
    systemFS.rmdir('/vendor');
    const h=systemFS.opendir('/');const rootFiles=[];let entry;
    try{while((entry=systemFS.readdir(h))!==null){const path='/'+entry.name;if((systemFS.lstat(path)?.mode&0xf000)===0x8000)rootFiles.push(path);}}finally{systemFS.closedir(h);}
    for(const path of rootFiles)move(path,legacyPath(path));
    writeFile('/etc/.wasm-posix-layout-v2','2\n');
  }
  // Seed once so later shell deletions remain deletions after reopening.
  seedFiles(Object.entries(data.files),'/etc/.wasm-posix-seeded');
  if(!data.hosted&&typeof migrateLegacyPackageSources==='function')
    await migrateLegacyPackageSources(data,{systemFS,readFile,writeFile,output,decoder,encoder});
  if(data.hosted)for(const [path,text] of Object.entries(data.edited||{}))writeFile(path,text);
  planBootDownloads(data);
  for(const [path,url] of Object.entries(data.dataFiles||{}))if(!systemFS.lstat(path))writeFile(path,await rawAsset(url));
  let sources;
  const bootSources=()=>sources||(sources=snapshot());
  // A missing boot executable is written from the bundle's prebuilt bytes
  // when it has them (tools/prebuilt.mjs), with the project cache key a
  // browser build would have left, and compiled from source otherwise.
  const seedPrebuilt=async out=>{
    const entry=data.prebuilt?.[out];
    if(!entry||systemFS.stat(out))return !!systemFS.stat(out);
    writeFile(out,entry.url?await rawAsset(entry.url):entry.bytes,0o755);
    if(entry.key)writeFile('/var/cache/wasm-posix/'+out.slice(1).replace(/\//g,'_')+'.key',entry.key);
    return true;
  };
  // Seed a coherent runtime pair on a fresh disk. Installed packages keep their
  // own producer/ABI manifest and survive compiler-only upgrades.
  const libcInfo='/usr/lib/wasm-posix/libc.json';
  if(needsCoreLibc(data)){
    if(data.prebuilt?.[libcInfo]){
      const paths=['/usr/lib/libc.a','/usr/lib/libc.so.1','/usr/lib/libc-fork.so.1',libcInfo];
      // Fetch and verify the entire pair before publishing any of it. A failed
      // download must not leave a half-installed runtime on the next boot.
      const bytes=await Promise.all(paths.map(async path=>{
        const entry=data.prebuilt[path];if(!entry)throw new Error('Incomplete prebuilt runtime');
        return entry.url?await rawAsset(entry.url):entry.bytes;
      }));
      for(let i=0;i<paths.length;i++)writeFile(paths[i],bytes[i],0o644);
    }else{
      const result=await compilerJobs.run({kind:'command',argv:['cc','--build-libc'],cwd:'/'});
      if(result.exitCode)throw new Error(result.stderr);
    }
  }
  for(const [name,json] of [['ash','ash.json'],['busybox','coreutils.json']]){
    if(await seedPrebuilt('/bin/'+name))continue;
    const result=await compilerJobs.run({kind:'project',sources:bootSources(),manifest:'/usr/src/busybox/'+json,out:'/bin/'+name,notice:`Building BusyBox ${name} from C source…\n`});
    if(result)postMessage({type:'built',name,stats:result.stats});
  }
  await seedPrebuilt('/usr/bin/make');
  // gucman and its private companion are one bundle-owned interface. Publish
  // the current helper first, including on existing disks. Keep the public
  // guc-links executable and editable source under their existing seed policy.
  // Fetch failure leaves the previous manager untouched; never compile this
  // companion from a potentially older or user-edited installed source.
  if(data.repository){
    await installRepositoryManager(data,rawAsset,writeFile,readFile);
  }else{
    const linksCompanion=data.prebuilt?.['/usr/bin/guc-links'];
    if(!linksCompanion)throw new Error('Missing prebuilt gucman companion');
    const linksBytes=linksCompanion.url?await rawAsset(linksCompanion.url):linksCompanion.bytes;
    writeFile('/usr/libexec/guc-links-v2',linksBytes,0o755);
    writeFile('/usr/bin/gucman',packageManagerScript({sourceArchives:data.sourceArchives||{},packageLicenses:data.packageLicenses||{},keys:data.packageKeys||{},artifacts:data.artifacts||'',toolchain:data.toolchain||{},versions:data.versions?.packages||{}}),0o755);
    for(const [name,pkg,...extra] of LAUNCHERS)writeFile('/usr/bin/'+name,launcherScript(name,pkg,...extra),0o755);
  }
  if(typeof initializeLegacyPackageDefaults==='function')
    initializeLegacyPackageDefaults({systemFS,readFile,writeFile,decoder});
  // /bin/sh is ash. Older disks carry hush as a regular /bin/sh file; replace it with the link.
  const sh=systemFS.lstat('/bin/sh');
  if(sh&&(sh.mode&0xf000)!==0xa000)systemFS.unlink('/bin/sh');
  if(!systemFS.lstat('/bin/sh'))systemFS.symlink('/bin/ash','/bin/sh');
  if(!await seedPrebuilt('/bin/cc'))await compilerJobs.run({kind:'program',sources:bootSources(),inputs:['/usr/libexec/cc-driver.c'],out:'/bin/cc'});
  writeFile('/usr/bin/nm', '#!/bin/sh\nexec /bin/cc --nm "$@"\n', 0o755);
  // Toolchain companions: ar builds static archives; ranlib has nothing to index.
  if(!systemFS.lstat('/usr/libexec/ar.c')&&data.files['/usr/libexec/ar.c']){writeFile('/usr/libexec/ar.c',data.files['/usr/libexec/ar.c']);if(sources)sources['/usr/libexec/ar.c']=data.files['/usr/libexec/ar.c'];}
  if(!await seedPrebuilt('/usr/bin/ar')&&systemFS.stat('/usr/libexec/ar.c')){await compilerJobs.run({kind:'program',sources:bootSources(),inputs:['/usr/libexec/ar.c'],out:'/usr/bin/ar'});}
  // guc-fetch downloads prebuilt package artifacts for gucman (examples/fetch.c).
  if(!systemFS.lstat('/usr/libexec/fetch.c')&&data.files['/usr/libexec/fetch.c']){writeFile('/usr/libexec/fetch.c',data.files['/usr/libexec/fetch.c']);if(sources)sources['/usr/libexec/fetch.c']=data.files['/usr/libexec/fetch.c'];}
  if(!await seedPrebuilt('/usr/bin/guc-fetch')&&systemFS.stat('/usr/libexec/fetch.c')){await compilerJobs.run({kind:'program',sources:bootSources(),inputs:['/usr/libexec/fetch.c'],out:'/usr/bin/guc-fetch'});}
  if(!systemFS.lstat('/usr/libexec/guc-links.c')&&data.files['/usr/libexec/guc-links.c']){writeFile('/usr/libexec/guc-links.c',data.files['/usr/libexec/guc-links.c']);if(sources)sources['/usr/libexec/guc-links.c']=data.files['/usr/libexec/guc-links.c'];}
  if(!await seedPrebuilt('/usr/bin/guc-links')&&systemFS.stat('/usr/libexec/guc-links.c')){await compilerJobs.run({kind:'program',sources:bootSources(),inputs:['/usr/libexec/guc-links.c'],out:'/usr/bin/guc-links'});}
  writeFile('/usr/bin/ranlib','#!/bin/sh\n# Archives carry no symbol index; the linker scans members.\nexit 0\n',0o755);
  for(const [path,exe] of Object.entries(data.executables||{}))writeFile(path,exe,0o755);
  // Dispatch the vendored multicall applets by argv[0].
  for(const name of data.applets){if(!systemFS.lstat('/bin/'+name))systemFS.symlink('/bin/busybox','/bin/'+name);}
  // Upstream scripts commonly use this absolute interpreter path.
  if(!systemFS.lstat('/usr/bin/env'))systemFS.symlink('/bin/env','/usr/bin/env');
  if(!systemFS.stat('/etc/passwd'))writeFile('/etc/passwd','user:x:1000:1000:Browser user:/home/user:/bin/sh\n');
  bootStatus('Starting your shell…');
  systemFS._s.flush();
  kernel=new KERNEL.Kernel({onClipboard:clip=>postMessage({type:"clipboard",text:clip?.fmt===1?decoder.decode(clip.bytes):""}),keepAlive:data.hosted,onProcessExit:(pid,status)=>{
    for(const [id,r] of terminalSessions)if(r.pid===pid){postMessage({type:'terminal-exit',terminalId:id,status});}
    for(const [id,r] of execSessions)if(r.pid===pid){clearTimeout(r.timer);execSessions.delete(id);stopTty(r.tty);systemFS._s.flush();postMessage({type:'rpc-result',id,value:{code:status>>>8,signal:status&127,output:r.output}});}
    if(data.hosted&&pid===commandPid){commandPid=0;systemFS._s.flush();postMessage({type:'command-exit',status,diskBytes:diskSize()});}
  },fs:new BLOCK_FS.MountFS({'/':systemFS,'/proc':new KERNEL.ProcFS()}),
    createWorker:spec=>{
      if(spec.thread){
        // Threads share the process's window and terminal; only the kernel sees them.
        const worker=new Worker(processURL,{name:`thread-${spec.pid}-${spec.tid}`});
        let listener=()=>{},exitListener=()=>{};
        const handle={postMessage:(msg,transfers)=>worker.postMessage(msg,transfers),onMessage:fn=>listener=fn,onExit:fn=>exitListener=fn,terminate:()=>worker.terminate()};
        worker.onmessage=({data:msg})=>{if(msg.type!=='window')listener(msg);};
        worker.onerror=e=>{listener({type:'crashed',error:e.message});exitListener(1);};
        worker.postMessage({type:'start',spec});return handle;
      }
      const worker=new Worker(processURL,{name:`busybox-${spec.pid}-${spec.path}`});
      let listener=()=>{},exitListener=()=>{};
      const handle={postMessage:(msg,transfers)=>worker.postMessage(msg,transfers),onMessage:fn=>listener=fn,onExit:fn=>exitListener=fn,terminate:()=>{worker.terminate();workerHandles.delete(spec.pid);postMessage({type:'process-exit',pid:spec.pid});}};
      worker.onmessage=({data:msg})=>{if(msg.type==='image-replaced'){postMessage({type:'process-exit',pid:spec.pid});postMessage({type:'process',pid:spec.pid,path:msg.path});}else if(msg.type==='window')postMessage({type:'window',pid:spec.pid,event:msg.event});else listener(msg);};
      worker.onerror=e=>{listener({type:'crashed',error:e.message});exitListener(1);};
      workerHandles.set(spec.pid,handle);postMessage({type:'process',pid:spec.pid,path:spec.path});worker.postMessage({type:'start',spec},spec.forkSnapshot?[spec.forkSnapshot.memory]:[]);return handle;
    },
    loadImage:path=>readFile(path),compile:(argv,cwd,context)=>compilerJobs.run({kind:'command',argv,cwd},context),
    commit:()=>commitSoon(),
    onOutput:(pid,fd,bytes)=>{
      const process=kernel.process(pid),terminal=process?.launchTty||process?.tty;
      for(const [id,r] of terminalSessions)if(r.tty===terminal){postMessage({type:'terminal-output',terminalId:id,text:r.decoder.decode(bytes,{stream:true})});return;}
      for(const r of execSessions.values())if(r.tty===terminal){r.output+=r.decoder.decode(bytes,{stream:true});if(r.output.length>1000000){r.output=r.output.slice(0,1000000)+'\n[Output limit reached]';stopTty(terminal);}return;}
      output(decoder.decode(bytes));
    },
    onAudioStream:scheduleAudio,
    onHalt:status=>{clearInterval(audioTimer);audioTimer=null;compilerJobs.close();const current=snapshot();const changed=Object.fromEntries(Object.entries(current).filter(([path,text])=>data.files[path]!==text));const deleted=Object.keys(data.files).filter(path=>!systemFS.lstat(path));closeDisk();postMessage({type:'halt',status,files:changed,deleted,diskBytes:diskSize()});},
    log:text=>output(text+'\n')
  });
  if(data.screenWidth&&data.screenHeight)kernel.setScreen(data.screenWidth,data.screenHeight);
  tty=kernel.createTty({cols:data.cols,rows:data.rows,interactiveOut:!data.args.length,output:bytes=>output(decoder.decode(bytes))});
  await runCommand(data.args);
  postMessage({type:'ready',persistence});
}
self.onmessage=async({data})=>{
  try{
    if(data.type==='boot')await initialize(data);
    else if(data.type==='run')await runCommand(data.args);
    else if(data.type==='rpc'){
      try{
        let value;
        const a=data.args||{};
        if(data.op==='terminal-open'){
          if(terminalSessions.has(a.terminalId))throw Error('Terminal already exists');
          const id=a.terminalId;
          const r={decoder:new TextDecoder()};
          r.tty=kernel.createTty({makeDefault:false,cols:a.cols,rows:a.rows,interactiveOut:true,output:bytes=>postMessage({type:'terminal-output',terminalId:id,text:r.decoder.decode(bytes,{stream:true})})});
          terminalSessions.set(id,r);
          try{r.pid=await kernel.boot({path:'/bin/sh',argv:['/bin/sh'],cwd:HOME_DIRECTORY,envp:shellEnvironment(),tty:r.tty});}catch(e){terminalSessions.delete(id);throw e;}
          value={pid:r.pid};
        }else if(data.op==='terminal-close'){
          const r=terminalSessions.get(a.terminalId);if(r){stopTty(r.tty);terminalSessions.delete(a.terminalId);}
        }else if(data.op==='exec'){
          const r={output:'',decoder:new TextDecoder()};
          r.tty=kernel.createTty({makeDefault:false,interactiveOut:false});execSessions.set(data.id,r);
          try{r.pid=await kernel.boot({path:'/bin/sh',argv:['/bin/sh','-c',a.command],cwd:a.cwd||HOME_DIRECTORY,envp:shellEnvironment(),tty:r.tty});}
          catch(e){execSessions.delete(data.id);throw e;}
          r.tty.eof(true);
          r.timer=setTimeout(()=>stopTty(r.tty),Math.min(Math.max(a.timeout||120000,100),600000));
          return;
        }else if(data.op==='exec-stop'){
          const r=execSessions.get(a.id);if(r)stopTty(r.tty);
        }else if(data.op==='list-directory'){
          value=[];const h=systemFS.opendir(a.path);
          if(h!=null)try{let e;while((e=systemFS.readdir(h))!==null)if(e.name!=='.'&&e.name!=='..')value.push(e.name);}finally{systemFS.closedir(h);}
        }else if(data.op==='atomic-write'){
          const tmp=a.path+'.tmp';writeFile(tmp,a.bytes,a.mode||0o600);
          if(systemFS.rename(tmp,a.path)!==0)throw Error('Cannot save '+a.path);
          systemFS._s.flush();
        }else throw Error('Unknown operation');
        postMessage({type:'rpc-result',id:data.id,value});
      }catch(e){postMessage({type:'rpc-result',id:data.id,error:e.message});}
    }
    else if(data.type==='terminal-input')terminalSessions.get(data.terminalId)?.tty.input(encoder.encode(data.text));
    else if(data.type==='terminal-resize')terminalSessions.get(data.terminalId)?.tty.resize(data.cols,data.rows);
    else if(data.type==='input')tty?.input(encoder.encode(data.text));
    else if(data.type==='screen-size')kernel?.setScreen(data.width,data.height);
    else if(data.type==='terminal-size')tty?.resize(data.cols,data.rows);
    else if(data.type==='canvas')workerHandles.get(data.pid)?.postMessage({type:'canvas',canvas:data.canvas},[data.canvas]);
    else if(data.type==='clipboard')kernel?.clipSet(1,encoder.encode(data.text));
    else if(data.type==='process-input')workerHandles.get(data.pid)?.postMessage(data.event);
    else if(data.type==='kill')kernel?.kill(data.pid,9,0);
    else if(data.type==='metadata'){
      let value;
      if(data.path)value=systemFS.stat(data.path);
      else{
        value={files:[],directories:[]};
        const visit=path=>{const stat=systemFS.lstat(path);if(!stat)return;
          if((stat.mode&0xf000)!==0x4000){value.files.push(path);return;}
          value.directories.push(path);const h=systemFS.opendir(path);if(h==null)return;
          try{let entry;while((entry=systemFS.readdir(h))!==null)if(entry.name!=='.'&&entry.name!=='..')visit(normalize(path+'/'+entry.name));}finally{systemFS.closedir(h);}
        };visit('/');
      }
      postMessage({type:'metadata',id:data.id,value});
    }
    else if(data.type==='read-file'){const bytes=readFile(data.path);postMessage({type:'file',id:data.id,bytes});}
    else if(data.type==='write-file'){writeFile(data.path,data.bytes);systemFS._s.flush();postMessage({type:'written',id:data.id});}
    else if(data.type==='sync'){systemFS?._s.flush();postMessage({type:'synced',id:data.id});}
  }catch(error){if(data.id!=null){postMessage({type:'request-error',id:data.id,error:error.message});return;}try{closeDisk();}catch{}postMessage({type:'error',text:error.stack||String(error)});}
};
