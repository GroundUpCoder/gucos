// Shell compiler driver. Filesystem helpers are supplied by the owner RPC bridge.
// Share backend option decoding between project manifests and ordinary cc.
function compilerFlag(arg, options) {
  if(arg==='-s'){options.stripDebug=true;return true;}
  if(/^-(?:g[0-3]?|ggdb[0-3]?)$/.test(arg)){options.emitNames=arg!=='-g0'&&arg!=='-ggdb0';return true;}
  if(arg==='-msimd128'||arg==='-mno-simd128'){options.simd128=arg==='-msimd128';return true;}
  if(arg==='-msse'||arg==='-msse2'){options.simd128=true;options.sse=true;if(arg==='-msse2')options.sse2=true;return true;}
  if(arg==='-mno-sse'||arg==='-mno-sse2'){options.sse2=false;if(arg==='-mno-sse')options.sse=false;return true;}
  if(arg==='--enable-fork'){options.enableFork=true;return true;}
  if(arg==='-fno-inline'||arg==='-finline'){options.noInline=arg==='-fno-inline';return true;}
  const disabled={'--no-ssa-memory':'ssaMemory','--no-ssa-sccp':'ssaSCCP','--no-ssa-induction':'ssaInduction','--no-ssa':'ssa','--no-native-functions':'nativeFunctions','--no-native-shared-functions':'nativeSharedFunctions','--no-wast-inline':'wastInline','--no-jspi-codegen':'jspi'};
  if(Object.hasOwn(disabled,arg)){options[disabled[arg]]=false;return true;}
  const flags = {
    '--ssa': ['ssa'],
    '--ssa-licm': ['ssaLICM'],
    '--verify-ssa': ['verifySSA'],
    '--allow-old-c': ['allowImplicitInt', 'allowKnRDefinitions', 'allowImplicitFunctionDecl'],
    '--allow-implicit-int': ['allowImplicitInt'],
    '--allow-knr-definitions': ['allowKnRDefinitions'],
    '--allow-implicit-function-decl': ['allowImplicitFunctionDecl'],
    '--allow-undefined': ['allowUndefined'],
    '--allow-zero-length-arrays': ['allowZeroLengthArrays'],
    '--gc-spill-locals': ['gcSpillLocals'],
  };
  if (!Object.hasOwn(flags, arg)) return false;
  for (const name of flags[arg]) options[name] = true;
  return true;
}
function project(config, root, files){
  const sources=[],includes=[],defines=[],compilerOptions={},sourceRoots=[];
  const join=(base,p)=>normalize(base+'/'+p);
  // Manifests may depend on manifests in sibling trees; each dependency's
  // paths resolve against its own directory.
  function add(c,base){
    for(const dep of c.deps||[]){const path=join(base,dep);const text=files[path];if(!text)throw new Error(`No such manifest: ${path}`);add(JSON.parse(text),path.slice(0,path.lastIndexOf('/')));}
    sources.push(...(c.sources||[]).map(p=>join(base,p)));includes.push(...(c.includes||[]).map(p=>join(base,p)));
    for(const [prefix,dir] of Object.entries(c.srcRoots||{}))sourceRoots.push({prefix,dir:join(base,dir)});
    for(const arg of c.compilerArgs||[]){if(arg.startsWith('-D'))defines.push(arg.slice(2));else compilerFlag(arg,compilerOptions);}
  }
  add(config,root);return {sources:[...new Set(sources)],includes:[...new Set(includes)],defines,compilerOptions,sourceRoots};
}
// Programs built from a vendored manifest are cached on disk under a key of the
// compiler build id plus every source the manifest can see; cc --project uses it.
let buildId='';
let compilerToolchain=null;
const encodeHex=bytes=>[...new Uint8Array(bytes)].map(b=>b.toString(16).padStart(2,'0')).join('');
function projectKey(root,sources){
  const texts=Object.keys(sources).filter(path=>path===root||path.startsWith(root+'/')).sort().map(path=>path+'\0'+sources[path]);
  return buildId+'\n'+hashText(texts.join('\n'));
}
// Stable cache key, computed in the compiler worker.
function hashText(text){
  let h1=0xcbf29ce4,h2=0x84222325;
  for(let i=0;i<text.length;i++){const c=text.charCodeAt(i);h1^=c>>>8;h2^=c&255;
    // 64-bit multiply by the FNV prime 0x100000001b3, split across two 32-bit halves.
    const lo=(h2>>>0)*0x1b3,hi=(h1>>>0)*0x1b3+(h2>>>0)+Math.floor(lo/0x100000000);
    h2=lo>>>0;h1=hi>>>0;}
  return h1.toString(16).padStart(8,'0')+h2.toString(16).padStart(8,'0');
}
function buildProject(sources,manifest,out,writeErr,onBuild=()=>{},compilerOptions={},force=false){
  const root=manifest.slice(0,manifest.lastIndexOf('/'));
  const config=sources[manifest];if(!config)throw new Error(`No such manifest: ${manifest}`);
  const p=project(JSON.parse(config),root,sources);
  Object.assign(p.compilerOptions,compilerOptions);
  const key=force?null:projectKey(root,sources)+'\n'+JSON.stringify(p.compilerOptions),keyPath='/var/cache/wasm-posix/'+out.slice(1).replace(/\//g,'_')+'.key',existing=readFile(keyPath);
  if(!force&&existing&&decoder.decode(existing)===key&&systemFS.stat(out))return null;
  onBuild();
  const result=compile(sources,p.sources,p.includes,writeErr,{defines:p.defines,compilerOptions:p.compilerOptions,sourceRoots:p.sourceRoots,libcArchive:p.compilerOptions.sharedLibc?readFile(LIBC_PATH):undefined});
  writeFile(out,result.bytes,0o755);systemFS.chmod(out,0o755);if(!force)writeFile(keyPath,key);
  return result;
}
// Sources the compiler reads on demand from the disk (an include search
// touches many paths; reading the whole tree per cc call is not needed).
function lazySources(){
  const cache=new Map();
  return new Proxy({},{get(_,path){
    if(typeof path!=='string')return undefined;
    if(cache.has(path))return cache.get(path);
    const stat=systemFS.stat(path);let text=undefined;
    if(stat&&(stat.mode&0xf000)===0x8000){const bytes=readFile(path);if(bytes){text=self.CompilerJS.decodeCompilerSource(bytes);}}
    cache.set(path,text);return text;
  },has(_,path){return typeof path==='string'&&!!systemFS.stat(path);}});
}
// Runtime compatibility follows ABI epochs, not the exact compiler producer.
// Ordinary compilation only consumes installed binaries. Rebuild is explicit.
const LIBC_PATH='/usr/lib/libc.a';
const LIBC_SHARED_PATH='/usr/lib/libc.so.1';
const LIBC_FORK_PATH='/usr/lib/libc-fork.so.1';
const LIBC_INFO_PATH='/usr/lib/wasm-posix/libc.json';
function ensureLibc(writeErr){
  let info;
  try{info=JSON.parse(decoder.decode(readFile(LIBC_INFO_PATH)));}catch{}
  const abi=compilerToolchain?.abi;
  if(!abi||!info?.abi||info.abi.objects!==abi.objects||info.abi.runtime!==abi.runtime)
    throw new Error('Missing or incompatible libc SDK; run gucman upgrade libc, or cc --build-libc to rebuild explicitly');
  const version=s=>String(s||'0').split('.').map(Number);
  const need=version(info.runtimeVersion),have=version(compilerToolchain.runtimeVersion);
  for(let i=0;i<3;i++){
    if((need[i]||0)>(have[i]||0))throw new Error('Installed libc requires a newer runtime');
    if((need[i]||0)<(have[i]||0))break;
  }
  if(!systemFS.stat(LIBC_PATH))throw new Error('Missing libc.a; run gucman upgrade libc');
  return false;
}
function ensureSharedLibc(writeErr,force=false){
  if(force){
    if(!compilerToolchain?.abi)throw new Error('Missing compiler ABI configuration');
    const archive=buildLibc(writeErr);
    const shared=buildSharedLibc(archive.bytes,writeErr);
    const forkShared=buildSharedLibc(archive.bytes,writeErr,true);
    writeFile(LIBC_PATH,archive.bytes);writeFile(LIBC_SHARED_PATH,shared.bytes);writeFile(LIBC_FORK_PATH,forkShared.bytes);
    writeFile(LIBC_INFO_PATH,JSON.stringify(compilerToolchain)+'\n');
    return;
  }
  ensureLibc(writeErr);
  for(const path of [LIBC_SHARED_PATH,LIBC_FORK_PATH])
    if(!systemFS.stat(path))throw new Error('Missing '+path.split('/').pop()+'; run gucman upgrade libc');
}
// Preprocessed output for cc -E: tokens with line markers.
// Configure scripts also open/stat the headers named by those markers. Publish
// the compiler-owned copies at their reserved absolute paths, without changing
// include search order or putting stale copies in /usr/include.
function ensureBuiltinHeaders(){
  const keyPath='/var/cache/wasm-posix/headers.key',key=buildId+'\nheaders-1';
  const previous=readFile(keyPath);
  if(previous&&decoder.decode(previous)===key)return;
  for(const [name,source] of Object.entries(self.CompilerJS.getStdlibHeaders()))
    writeFile('/__wasm_posix__/include/'+name,source);
  writeFile(keyPath,key);
}
function preprocessText(files,input,includes,extra){
  const C=self.CompilerJS,pp=preprocessor(files,includes,extra);
  const source=files[input];if(source===undefined)throw new Error(`No such source: ${input}`);
  const result=C.tokenize(input,source,pp);
  if(result.errors.length)throw new Error(result.errors.map(e=>`${e.filename}:${e.line}: error: ${e.message}`).join('\n'));
  let out='',file=null,line=0;
  for(const t of result.tokens){
    if(t.kind==='EOS')break;
    if(t.kind==='NEWLINE'||t.kind==='PLACEMARKER')continue;
    if(t.filename!==file){file=t.filename;line=t.line;out+=`${out&&!out.endsWith('\n')?'\n':''}# ${line} "${file}"\n`;}
    else if(t.line!==line){if(t.line>line&&t.line-line<=8)out+='\n'.repeat(t.line-line);else out+=`\n# ${t.line} "${file}"\n`;line=t.line;}
    else if(t.hasSpace&&out&&!out.endsWith('\n'))out+=' ';
    out+=t.text;
  }
  return out+'\n';
}
const CC_VERSION='wasm-posix cc 1.0 (wasm32-unknown-wasmposix)';
const CC_TARGET='wasm32-unknown-wasmposix';
// Libraries supplied by libc (shared or static); their -l flags need no extra input.
const BUILTIN_LIBS=new Set(['c','m','pthread','dl','rt','util','crypt','gcc','gcc_s']);
// Options taken for compatibility with other drivers and ignored.
const IGNORED_OPTION=/^-(O[0-9sz]?$|g[0-9]?$|ggdb|W|f|m[a-z]|std=|pedantic|pthread$|static$|shared$|s$|w$|ansi$|nostdlib|nostartfiles|rdynamic$|pipe$|pg$|p$|Q|no-pie$|pie$|fPIC$)/;
// BSD-style symbol listing for our relocatable objects and ar containers.
// Values are object-relative data offsets or function indices, not ELF addresses.
function nmCommand(argv,cwd){
  let stdout='',stderr='',exitCode=0,external=false,defined=false,undefinedOnly=false,namesOnly=false,prefix=false,posix=false;
  const paths=[];
  try{
    for(const arg of argv){
      if(arg==='--version'||arg==='-V')return {exitCode:0,stdout:'wasm-posix nm 1.0\n',stderr:''};
      if(arg==='--defined-only')defined=true;
      else if(arg==='--extern-only')external=true;
      else if(arg==='--undefined-only')undefinedOnly=true;
      else if(arg==='--') {paths.push(...argv.slice(argv.indexOf(arg)+1));break;}
      else if(arg.startsWith('-')){
        for(const flag of arg.slice(1)){
          if(flag==='g')external=true;
          else if(flag==='u')undefinedOnly=true;
          else if(flag==='j')namesOnly=true;
          else if(flag==='A'||flag==='o')prefix=true;
          else if(flag==='P')posix=true;
          else if(flag!=='B')throw Error('unsupported option: '+arg);
        }
      }else paths.push(arg);
    }
    if(!paths.length)paths.push('a.out');
    const list=(bytes,name,heading)=>{
      if(new TextDecoder().decode(bytes.subarray(0,8))==='!<arch>\n'){
        for(const member of self.WasmObjects.readArchive(bytes,name))list(member.bytes,name+'['+member.name+']',true);
        return;
      }
      const obj=self.WasmObjects.readObject(bytes,name),symbols=[];
      for(const [kind,items] of [['T',obj.info.funcSymbols],['D',obj.info.dataSymbols]])for(const sym of items){
        if(sym.flags&8)continue; // Host imports are not C linker symbols.
        const isDefined=!!(sym.flags&1),local=!!(sym.flags&2);
        if(external&&local||defined&&!isDefined||undefinedOnly&&isDefined)continue;
        symbols.push({name:sym.name,type:isDefined?(local?kind.toLowerCase():kind):'U',value:isDefined?(kind==='T'?sym.idx:sym.offset):null,size:sym.size||0});
      }
      symbols.sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0);
      if(heading&&!prefix)stdout+='\n'+name+':\n';
      for(const sym of symbols){
        const value=sym.value===null?'        ':sym.value.toString(16).padStart(8,'0');
        stdout+=(prefix?name+':':'')+(namesOnly?sym.name:posix?sym.name+' '+sym.type+' '+(sym.value===null?'':sym.value.toString(16)+' '+sym.size.toString(16)):value+' '+sym.type+' '+sym.name)+'\n';
      }
    };
    for(const name of paths){
      try{
        const path=normalize(name.startsWith('/')?name:cwd+'/'+name),bytes=readFile(path);
        if(!bytes)throw Error(name+': No such file or directory');
        list(bytes,name,paths.length>1);
      }catch(error){stderr+='nm: '+error.message+'\n';exitCode=1;}
    }
  }catch(error){stderr+='nm: '+error.message+'\n';exitCode=1;}
  return {exitCode,stdout,stderr};
}
// Response files keep large native-style link commands out of the syscall payload.
// Paths, including nested @files, are relative to the command's working directory.
function expandCompilerResponses(argv,cwd){
  const output=[argv[0]],active=new Set();
  function expand(arg){
    if(!arg.startsWith('@')){output.push(arg);return;}
    const path=normalize(arg[1]==='/'?arg.slice(1):cwd+'/'+arg.slice(1));
    if(active.has(path))throw new Error('cc: response file cycle: '+path);
    if(active.size>=64)throw new Error('cc: response file nesting exceeds 64: '+path);
    const bytes=readFile(path);
    if(!bytes)throw new Error('cc: missing response file: '+path);
    const text=decoder.decode(bytes);
    active.add(path);
    try{
      let word='',quote=null,started=false;
      for(let i=0;i<text.length;i++){
        const ch=text[i];
        if(ch==='\\'){
          if(++i===text.length)throw new Error('cc: trailing escape in response file: '+path);
          word+=text[i];started=true;
        }else if(quote){
          if(ch===quote)quote=null;else word+=ch;
        }else if(ch==='"'||ch==="'"){
          quote=ch;started=true;
        }else if(/\s/.test(ch)){
          if(started){expand(word);word='';started=false;}
        }else{word+=ch;started=true;}
      }
      if(quote)throw new Error('cc: unterminated quote in response file: '+path);
      if(started)expand(word);
    }finally{active.delete(path);}
  }
  for(let i=1;i<argv.length;i++)expand(argv[i]);
  return output;
}
// cc compiles relocatable objects and defaults to the shared process runtime.
// -static retains libc.a; direct owner-side project builds retain the boot path.
function compileCommand(argv,cwd){
  let stdout='',stderr='';
  const writeErr=text=>stderr+=text;
  try{
    argv=expandCompilerResponses(argv,cwd);
    if(argv[1]==='--nm')return nmCommand(argv.slice(2),cwd);
    if(argv.length===2&&argv[1]==='--build-libc'){ensureSharedLibc(writeErr,true);systemFS._s.flush();return {exitCode:0,stdout,stderr};}
    if(argv.length===3&&argv[1]==='--install-sdk-headers'){
      ensureLibc(writeErr);
      const prefix=normalize(argv[2].startsWith('/')?argv[2]:cwd+'/'+argv[2]);
      for(const [name,source] of Object.entries(self.CompilerJS.getStdlibHeaders()))writeFile(prefix+'/include/'+name,source);
      writeFile(prefix+'/share/wasm-posix/sdk.json',JSON.stringify(compilerToolchain)+'\n');
      systemFS._s.flush();return {exitCode:0,stdout,stderr};
    }
    const inputs=[],includes=[],defines=[],undefines=[],libs=[],libDirs=[],executionProviders=[],compilerOptions={};
    let forceProject=false,staticLink=false,forceStatic=false;
    compilerOptions.runpath=[];
    let out=null,manifest=null,mode='link',depFile=null,depTarget=null,writeDeps=false,phonyDeps=false,systemDeps=false;
    const resolve=path=>normalize(path.startsWith('/')?path:cwd+'/'+path);
    for(let i=1;i<argv.length;i++){
      const arg=argv[i];
      const value=flag=>{if(arg.length>flag.length)return arg.slice(flag.length);if(i+1>=argv.length)throw new Error(`cc: missing argument to '${flag}'`);return argv[++i];};
      if(arg==='-o')out=resolve(value('-o'));
      else if(arg==='--execution-provider')executionProviders.push(resolve(value(arg)));
      else if(arg.startsWith('--execution-provider='))executionProviders.push(resolve(arg.slice('--execution-provider='.length)));
      else if(arg==='-shared'){compilerOptions.shared=true;}
      else if(arg==='-static')forceStatic=true;
      else if(arg.startsWith('-Wl,')){
        const flags=arg.slice(4).split(',');
        for(let j=0;j<flags.length;j++){
          const f=flags[j];
          if(f==='-s'||f==='--strip-all'||f==='--strip-debug')compilerOptions.stripDebug=true;
          else if(f==='-Bstatic')staticLink=true;
          else if(f==='-Bdynamic')staticLink=false;
          else if(f==='-soname'||f==='--soname'){if(!flags[j+1])throw new Error('Missing soname');compilerOptions.soname=flags[++j];}
          else if(f==='-rpath'||f==='--rpath'){if(!flags[j+1])throw new Error('Missing rpath');compilerOptions.runpath.push(...flags[++j].split(':'));}
          else throw new Error('Unsupported linker option: '+f);
        }
      }
      else if(arg==='-rdynamic'){compilerOptions.dynamic=true;}
      else if(arg==='-c')mode='object';
      else if(arg==='-E')mode='preprocess';
      else if(arg==='-M'||arg==='-MM'){mode='deps';systemDeps=arg==='-M';}
      else if(arg==='-MD'||arg==='-MMD'){writeDeps=true;systemDeps=arg==='-MD';}
      else if(arg==='-MF')depFile=resolve(value('-MF'));
      else if(arg==='-MT'||arg==='-MQ')depTarget=value(arg);
      else if(arg==='-MP')phonyDeps=true;
      else if(arg==='--rebuild')forceProject=true;
      else if(arg==='--project')manifest=resolve(value('--project'));
      else if(arg.startsWith('-I'))includes.push(resolve(value('-I')));
      else if(arg.startsWith('-D'))defines.push(value('-D'));
      else if(arg.startsWith('-U'))undefines.push(value('-U'));
      else if(arg.startsWith('-l'))libs.push({name:value('-l'),static:staticLink});
      else if(arg.startsWith('-L'))libDirs.push(resolve(value('-L')));
      else if(arg==='-x'||arg==='-include'||arg==='-isystem'||arg==='-idirafter'){const v=value(arg);if(arg!=='-x')includes.push(resolve(v));}
      else if(arg==='--version'||arg==='-v'||arg==='-V'){stdout+=CC_VERSION+'\n';return {exitCode:0,stdout,stderr};}
      else if(arg==='-dumpmachine'||arg==='-print-multiarch'){stdout+=CC_TARGET+'\n';return {exitCode:0,stdout,stderr};}
      else if(arg==='-dumpversion'){stdout+='1.0\n';return {exitCode:0,stdout,stderr};}
      else if(arg.startsWith('-print-')){stdout+='\n';return {exitCode:0,stdout,stderr};}
      else if(arg==='-S')throw new Error('cc: -S (assembly output) is not supported; use -c for an object');
      else if(compilerFlag(arg,compilerOptions)){}
      else if(IGNORED_OPTION.test(arg)){}
      else if(arg.startsWith('-'))throw new Error(`cc: unsupported option ${arg}`);
      else inputs.push(resolve(arg));
    }
    // Project flags must be known before selecting the process runtime and
    // plugin contracts. Command-line options still override manifest options.
    if(manifest){
      const sources=lazySources(),config=sources[manifest];
      if(!config)throw new Error('No such manifest: '+manifest);
      const inherited=project(JSON.parse(config),manifest.slice(0,manifest.lastIndexOf('/')),sources).compilerOptions;
      Object.assign(compilerOptions,{...inherited,...compilerOptions});
    }
    const sharedLibraries=[];
    const addShared=(path,bytes)=>{
      let meta;
      try{meta=self.WasmObjects.readSharedLibrary(bytes);}catch(e){throw new Error(path+': '+e.message);}
      const name=meta.soname||path.split('/').pop();
      if(!sharedLibraries.some(l=>l.name===name))sharedLibraries.push({name,symbols:meta.symbols});
    };
    const libraryArchives=[];
    for(const lib of mode==='link'?libs:[]){
      if(BUILTIN_LIBS.has(lib.name))continue;
      const dirs=[...libDirs,'/usr/local/lib','/usr/lib'];
      const found=dirs.flatMap(d=>forceStatic||lib.static?[`${d}/lib${lib.name}.a`]:[`${d}/lib${lib.name}.so`,`${d}/lib${lib.name}.a`]).find(p=>systemFS.stat(p));
      if(!found)throw new Error(`cc: cannot find -l${lib.name}`);
      if(found.endsWith('.so'))addShared(found,readFile(found));else libraryArchives.push({name:found,bytes:readFile(found)});
    }
    if(mode==='link'&&!compilerOptions.shared&&!forceStatic){
      ensureSharedLibc(writeErr);
      const runtimePath=compilerOptions.enableFork?LIBC_FORK_PATH:LIBC_SHARED_PATH;
      addShared(runtimePath,readFile(runtimePath));
      compilerOptions.sharedLibc=true;compilerOptions.dynamic=true;
    }
    if(mode==='link'&&compilerOptions.shared&&compilerOptions.jspi!==false&&compilerOptions.nativeSharedFunctions!==false){
      // Plugins bind the process's one runtime, including executable-owned
      // libc. Use its checked contracts without adding a second runtime DSO.
      ensureSharedLibc(writeErr);
      compilerOptions.runtimeContracts=self.WasmObjects.readSharedLibrary(readFile(compilerOptions.enableFork?LIBC_FORK_PATH:LIBC_SHARED_PATH)).symbols;
    }
    if(mode==='link')for(const path of executionProviders){
      const bytes=readFile(path);if(!bytes)throw new Error('Missing execution provider: '+path);
      const provider=self.WasmObjects.readSharedLibrary(bytes,false);
      if(provider.version<4)
        throw new Error('Legacy execution provider: '+path+'; relink it first');
      compilerOptions.runtimeContracts=[...(compilerOptions.runtimeContracts||[]),...provider.symbols];
    }
    compilerOptions.sharedLibraries=sharedLibraries;
    if(sharedLibraries.length)compilerOptions.dynamic=true;
    if(manifest){
      if(libraryArchives.length)throw new Error('Archive -l inputs with --project are not supported');
      if(inputs.length)throw new Error('cc: --project takes no source files');
      // Cache keys enumerate the project tree. Read dependencies lazily rather
      // than serializing every installed source tree into one worker message.
      const local=snapshot(manifest.slice(0,manifest.lastIndexOf('/'))), fallback=lazySources();
      const sources=new Proxy(local,{get(target,path){return Object.hasOwn(target,path)?target[path]:fallback[path];}});
      const result=buildProject(sources,manifest,out||resolve('a.out'),writeErr,()=>{},compilerOptions,forceProject);systemFS._s.flush();
      return {exitCode:0,stdout:result?`${result.stats.functions} functions, ${result.stats.bytes} bytes\n`:`${out||resolve('a.out')} is up to date\n`,stderr};
    }
    if(forceProject)throw new Error('cc: --rebuild requires --project');
    if(!inputs.length)throw new Error('cc: no input files');
    const files=lazySources();
    const isSource=path=>/\.(c|h|i)$/.test(path);
    const sources=inputs.filter(isSource);
    if(mode==='preprocess'){
      ensureBuiltinHeaders();
      let text='';for(const input of sources)text+=preprocessText(files,input,includes,{defines,undefines,compilerOptions});
      if(out)writeFile(out,text);else stdout+=text;
      systemFS._s.flush();return {exitCode:0,stdout,stderr};
    }
    // Dependency output (make): every file the preprocessor read.
    const depsFor=(input,target)=>{
      const read=new Set();
      compileObject(files,input,includes,()=>{},{defines,undefines,compilerOptions,onRead:path=>read.add(path)});
      read.delete(input);
      const list=[input,...read].filter(p=>systemDeps||!p.startsWith('/usr/include/'));
      let text=`${target}: ${list.join(' \\\n  ')}\n`;
      if(phonyDeps)for(const p of list.slice(1))text+=`\n${p}:\n`;
      return text;
    };
    const objectPath=input=>out&&mode==='object'?out:resolve(input.replace(/^.*\//,'').replace(/\.[ci]$/,'')+'.o');
    if(mode==='deps'){
      let text='';for(const input of sources)text+=depsFor(input,depTarget||objectPath(input).replace(/^.*\//,''));
      if(depFile)writeFile(depFile,text);else stdout+=text;
      systemFS._s.flush();return {exitCode:0,stdout,stderr};
    }
    if(mode==='object'&&out&&sources.length>1)throw new Error('cc: cannot specify -o with -c and multiple files');
    const objects=[];
    for(const input of inputs){
      if(isSource(input)){
        const object=compileObject(files,input,includes,writeErr,{defines,undefines,compilerOptions});
        const path=objectPath(input);
        if(mode==='object')writeFile(path,object.bytes);
        objects.push({name:input,bytes:object.bytes});
        if(writeDeps){const d=depFile||path.replace(/\.o$/,'.d');writeFile(d,depsFor(input,depTarget||path.replace(/^.*\//,'')));}
      }else{
        const bytes=readFile(input);if(!bytes)throw new Error(`cc: ${input}: No such file or directory`);
        objects.push({name:input,bytes});
      }
    }
    if(mode==='object'){systemFS._s.flush();return {exitCode:0,stdout,stderr};}
    // Link: command-line objects and archives, -l libraries, then libc.a.
    const archives=[...libraryArchives];
    const linkObjects=objects.filter(o=>{
      if(o.name.endsWith('.a')){archives.push(o);return false;}
      if(/\.so(?:\.[0-9.]+)?$/.test(o.name)){addShared(o.name,o.bytes);return false;}
      return true;
    });
    if(!compilerOptions.shared){if(ensureLibc(writeErr))stderr+='';archives.push({name:LIBC_PATH,bytes:readFile(LIBC_PATH)});}
    const result=linkProgram(linkObjects,archives,writeErr,{compilerOptions});
    const target=out||resolve('a.out');
    writeFile(target,result.bytes,0o755);systemFS.chmod(target,0o755);systemFS._s.flush();
    return {exitCode:0,stdout:`${result.stats.functions} functions, ${result.stats.bytes} bytes\n`,stderr};
  }catch(e){return {exitCode:1,stdout,stderr:stderr+e.message+'\n'};}
}
