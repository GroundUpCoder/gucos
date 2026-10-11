// Record reproducible ABI evidence from immutable prebuilt modules and headers.
// This is a review inventory; execution/relink tests remain required.
import {readFile,readdir,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
const root=process.argv[2]||'dist',output=process.argv[3]||'docs/research/platform-1.0/abi-inventory.json';
const build=JSON.parse(await readFile(root+'/build.json','utf8'));
const sha=b=>createHash('sha256').update(b).digest('hex');
const modules={};
const inventory=JSON.parse(await readFile(root+'/hosted-distribution.json','utf8'));
for(const path of Object.keys(inventory.assets).sort()){
 const name=path.slice('assets/'.length);
 if(!name.endsWith('.bin'))continue;const bytes=await readFile(root+'/assets/'+name);
 if(bytes.readUInt32LE(0)!==0x6d736100)continue;
 const module=new WebAssembly.Module(bytes);
 modules[name]={sha256:sha(bytes),imports:WebAssembly.Module.imports(module),exports:WebAssembly.Module.exports(module),objectMetadata:WebAssembly.Module.customSections(module,'wasmposix.obj').map(b=>sha(Buffer.from(b))),dynamicMetadata:WebAssembly.Module.customSections(module,'wasmposix.dylink').map(b=>sha(Buffer.from(b)))};
}
const manifest=JSON.parse(await readFile('libc/manifest.json','utf8')),headers={};
for(const name of manifest.headers)headers[name]=sha(await readFile('libc/include/'+name));
await mkdir(new URL('../docs/research/platform-1.0/',import.meta.url),{recursive:true});
await writeFile(output,JSON.stringify({runtime:build.runtime,platform:build.platform,toolchain:build.toolchain,headers,modules},null,2)+'\n');
console.log('Recorded '+Object.keys(modules).length+' module import/export inventories and '+Object.keys(headers).length+' header hashes');
