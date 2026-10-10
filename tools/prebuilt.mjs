import {runCompilerScript} from './compiler-context.mjs';
// Boot executables compiled on the host with the bundled compiler.
//
// A fresh browser disk used to spend about thirty seconds compiling ash,
// busybox, cc and ar before the first prompt. The bundler now compiles the
// same manifests with the same compiler in Node and ships the bytes in the
// payload; the system worker writes them instead of compiling when the
// target is missing. The compiler is deterministic, so the bytes match a
// browser build of the same sources. Results are cached under
// ~/.cache/wasm-posix/prebuilt by build id and source hash, so a bundle
// rebuild that leaves them untouched costs nothing.
import vm from 'node:vm';
import {readFile, writeFile, mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {homedir} from 'node:os';
import {join} from 'node:path';

export const BOOT_TARGETS=[
  {out:'/usr/bin/make', manifest:'/usr/src/make/bin.json'},
  {out:'/bin/ash', manifest:'/usr/src/busybox/ash.json'},
  {out:'/bin/busybox', manifest:'/usr/src/busybox/coreutils.json'},
  {out:'/bin/cc', inputs:['/usr/libexec/cc-driver.c']},
  {out:'/usr/bin/ar', inputs:['/usr/libexec/ar.c']},
  {out:'/usr/bin/guc-fetch', inputs:['/usr/libexec/fetch.c']},
  {out:'/usr/bin/guc-links', inputs:['/usr/libexec/guc-links.c']},
];
const cacheDir=process.env.WASM_POSIX_PREBUILT_CACHE||join(homedir(),'.cache','wasm-posix','prebuilt');

// sources: path -> text, as the system worker's boot snapshot sees them.
// Returns {out: {bytes: base64, key: project cache key or null}}.
export async function buildPrebuilt({sources, buildId, toolchain, root, log=()=>{}}){
  const hash=createHash('sha256').update('runtime-sdk-1\n'+buildId+'\n'+JSON.stringify(toolchain));
  for(const path of Object.keys(sources).sort())if((path.startsWith('/usr/src/busybox/')||path.startsWith('/usr/src/make/'))||path.startsWith('/usr/libexec/'))hash.update(path+'\0'+sources[path]+'\n');
  const cacheKey=hash.digest('hex'),cachePath=join(cacheDir,cacheKey+'.json');
  try{return JSON.parse(await readFile(cachePath,'utf8'));}catch{}
  const context=vm.createContext({console,TextEncoder,TextDecoder,performance,WebAssembly,decoder:new TextDecoder(),
    readFile:()=>undefined,writeFile:()=>{},snapshot:()=>sources,systemFS:{stat:()=>null,chmod(){},_s:{flush(){}}}});
  context.self=context;
  for(const name of ['path','compiler','trampoline','objects','compile','compiler-driver'])runCompilerScript(await readFile(new URL(`src/${name}.js`,root),'utf8'),context,{filename:name+'.js'});
  runCompilerScript(`buildId=${JSON.stringify(buildId)}`,context);
  const prebuilt={};
  const targets=[...BOOT_TARGETS];
  if(sources['/usr/libexec/guc-repo-json.c'])targets.push(
    {out:'/usr/libexec/guc-json',inputs:['/usr/libexec/guc-repo-json.c']},
    {out:'/usr/libexec/guc-unpack',inputs:['/usr/libexec/guc-repo-unpack.c']});
  for(const target of targets){
    const started=performance.now();
    let result,key=null;
    const errors=[];const writeErr=text=>errors.push(text);
    try{
    if(target.manifest){
      const manifestRoot=target.manifest.slice(0,target.manifest.lastIndexOf('/'));
      const p=context.project(JSON.parse(sources[target.manifest]),manifestRoot,sources);
      key=context.projectKey(manifestRoot,sources)+'\n'+JSON.stringify(p.compilerOptions);
      result=context.compile(sources,p.sources,p.includes,writeErr,{defines:p.defines,compilerOptions:p.compilerOptions,sourceRoots:p.sourceRoots});
    }else result=context.compile(sources,target.inputs,[],writeErr);
    }catch(error){process.stderr.write(`Prebuilt ${target.out} failed:\n${errors.join('')}`);throw error;}
    prebuilt[target.out]={bytes:Buffer.from(result.bytes).toString('base64'),key};
    log(`Prebuilt ${target.out}: ${result.stats.functions} functions, ${result.bytes.length} bytes, ${Math.round(performance.now()-started)} ms`);
  }
  if(sources['/usr/libexec/guc-repo-json.c']){
    prebuilt['/usr/libexec/guc-repo-fetch']=prebuilt['/usr/bin/guc-fetch'];
    prebuilt['/usr/libexec/guc-links-v2']=prebuilt['/usr/bin/guc-links'];
  }
  const libc=context.buildLibc(log),shared=context.buildSharedLibc(libc.bytes,log);
  const forkShared=context.buildSharedLibc(libc.bytes,log,true);
  prebuilt['/usr/lib/libc-fork.so.1']={bytes:Buffer.from(forkShared.bytes).toString('base64'),key:null};
  prebuilt['/usr/lib/libc.a']={bytes:Buffer.from(libc.bytes).toString('base64'),key:null};
  prebuilt['/usr/lib/libc.so.1']={bytes:Buffer.from(shared.bytes).toString('base64'),key:null};
  prebuilt['/usr/lib/wasm-posix/libc.json']={bytes:Buffer.from(JSON.stringify(toolchain)+'\n').toString('base64'),key:null};
  await mkdir(cacheDir,{recursive:true});
  await writeFile(cachePath+'.'+process.pid,JSON.stringify(prebuilt));
  const {rename}=await import('node:fs/promises');await rename(cachePath+'.'+process.pid,cachePath);
  return prebuilt;
}
