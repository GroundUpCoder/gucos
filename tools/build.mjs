// Fixed bootstrap only: this file never discovers package recipes or catalogs.
import {readFile,readdir,mkdir,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {loadLibc,libcModule} from '../src/libc-sources.js';
import {buildPrebuilt} from './prebuilt.mjs';
import {buildIdentity} from './build-identity.mjs';
import {hostedPayload} from './hosted-assets.mjs';
import {packBundle} from './bundle.mjs';
const root=new URL('../',import.meta.url),out=new URL('dist/',root);
const read=p=>readFile(new URL(p,root),'utf8');
const sha=b=>createHash('sha256').update(b).digest('hex');
const libc=loadLibc(),libcAsset=libcModule(libc);
const join=async names=>(await Promise.all(names.map(read))).join('\n');
const compilerJoin=async names=>(await Promise.all(names.map(async p=>(await read(p))+(p==='src/compiler.js'?'\n'+libcAsset:'')))).join('\n');
const files={};
async function collect(dir,prefix){for(const e of (await readdir(new URL(dir,root),{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){
 const p=dir+'/'+e.name,target=prefix+'/'+e.name;
 if(e.isSymbolicLink())throw Error('Bootstrap sources must be regular files: '+p);
 if(e.isDirectory())await collect(p,target);else files[target]=await read(p);
}}
await collect('vendor/busybox','/usr/src/busybox');
await collect('vendor/make','/usr/src/make');
for(const name of ['ar.c','cc-driver.c','fetch.c','guc-links.c'])files['/usr/libexec/'+name]=await read('examples/'+name);
for(const name of ['json','unpack','lock'])files['/usr/libexec/guc-repo-'+name+'.c']=await read('tools/guest-repository/'+name+'.c');
const srcNames=(await readdir(new URL('src/',root))).sort();
const buildId=sha(libc.manifest.digest+'\n'+await join(srcNames.map(n=>'src/'+n))+'\n'+JSON.stringify(files)+'\n'+await read('tools/versions.json')+'\n'+await read('tools/platform.json')+'\n'+await read('tools/guest-repository/manager.sh')+'\n'+await read('tools/guest-repository/generations.sh'));
const versions=JSON.parse(await read('tools/versions.json'));
const toolchain={...buildIdentity(versions,await join(['src/compiler.js','src/trampoline.js','src/objects.js','src/compile.js','src/compiler-driver.js','src/path.js'])),systemBuild:buildId,libcDigest:libc.manifest.digest};
const prebuilt=await buildPrebuilt({sources:files,buildId,toolchain,root,log:console.log});
await mkdir(out,{recursive:true});
const host=await join(['src/dynamic-loader.js','src/host.js']),kernel=await read('src/kernel.js');
const runtime=buildId;
const platform={...JSON.parse(await read('tools/platform.json')),version:versions.platform,runtimeAbi:versions.abi.runtime,objectAbi:versions.abi.objects};
const repository={format:1,url:process.env.PACKAGE_REPOSITORY||'https://raw.githubusercontent.com/GroundUpCoder/wasm-posix-packages/main/catalog.json',runtime,platform};
const notices={platform:{},packages:{},trees:{}};
for(const name of ['COMPILER-LICENSE','MUSL-LICENSE','CORE-MATH-LICENSE','UNICODE-LICENSE.txt'])notices.platform[name]=await read('vendor/'+name);
for(const name of ['XTERM-LICENSE','FIT-LICENSE'])notices.platform[name]=await read('vendor/terminal/'+name);
for(const [tree,name] of [['busybox','LICENSE'],['make','COPYING']])notices.packages[tree]=notices.trees[tree]=await read('vendor/'+tree+'/'+name);
notices.packages.make=notices.trees.make+='\n'+await read('vendor/make/COPYING.LIB');
const payload={buildId,toolchain,versions,prebuilt,repository,
 repositoryScripts:{manager:await read('tools/guest-repository/manager.sh'),generations:await read('tools/guest-repository/generations.sh')},
 systemCompiler:await compilerJoin(['src/path.js','src/compiler.js','src/trampoline.js','src/objects.js','src/compile.js','src/compiler-fs.js','src/compiler-driver.js','src/compiler-worker.js']),
 systemOwner:[host,kernel,await join(['src/path.js','src/file-locks.js','src/mapped-files.js','src/compiler-fs.js','src/compiler-jobs.js','src/system-files.js','src/journal.js','src/layout.js','src/packages.js','src/repository-boot.js','src/system-owner.js'])].join('\n'),
 systemProcess:[host,kernel,await join(['src/virtual-memory.js','src/system-process.js'])].join('\n'),
 worker:await compilerJoin(['src/path.js','src/compiler.js','src/trampoline.js','src/objects.js','src/dynamic-loader.js','src/host.js','src/compile.js','src/worker.js']),
 applets:[...(await read('vendor/busybox/port/multicall_main.c')).matchAll(/\{\s*"([a-zA-Z0-9_[\]-]+)"\s*,/g)].map(m=>m[1]),
 terminalJS:await join(['vendor/terminal/xterm.js','vendor/terminal/addon-fit.js']),terminalCSS:await read('vendor/terminal/xterm.css'),files,binaryFiles:{},licenseNotices:notices};
const hosted=await hostedPayload(payload,out);delete hosted.sourceArchives;
const app=await join(['src/startup.js','src/repository-boot.js','src/layout.js','src/windows.js','src/vi.js','src/terminal.js','src/list.js','src/system-audio.js','src/system-ui.js','src/reset.js','src/app.js']);
await writeFile(new URL('index.html',out),(await read('src/index.html')).replace('/* APP */',()=>app).replace('PAYLOAD',()=>packBundle(hosted)));
await writeFile(new URL('build.json',out),JSON.stringify({format:1,runtime,platform,toolchain,bootstrap:['libc','ash','busybox','cc','ar','make','guc-fetch','gucman']},null,2)+'\n');
console.log('Built self-contained dist/; runtime '+runtime+'; package source is configured only at runtime.');
