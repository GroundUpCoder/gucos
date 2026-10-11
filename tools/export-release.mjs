// Export only the current core build and its complete source/license asset closure.
import {readFile,mkdir,copyFile,writeFile,lstat} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {createHash} from 'node:crypto';
const [input,output]=process.argv.slice(2);if(!input||!output)throw Error('usage: node tools/export-release.mjs DIST NEW_RELEASE');
const root=resolve(input),out=resolve(output),inventory=JSON.parse(await readFile(join(root,'hosted-distribution.json')));
const names=['index.html','build.json','hosted-distribution.json',...Object.keys(inventory.assets)];
const files={};
for(const name of names){
 if(!/^(index\.html|build\.json|hosted-distribution\.json|assets\/[a-f0-9]{64}\.(js|bin|tar|txt|html))$/.test(name))throw Error('invalid release path');
 if(!(await lstat(join(root,name))).isFile())throw Error('nonregular release file');
 const bytes=await readFile(join(root,name));const sha256=createHash('sha256').update(bytes).digest('hex');
 if(name.startsWith('assets/')&&name.slice(7,71)!==sha256)throw Error('asset hash differs from filename');
 files[name]={sha256,size:bytes.length};
}
await mkdir(out,{recursive:false});await mkdir(join(out,'assets'));
for(const name of names)await copyFile(join(root,name),join(out,name));
const bytes=Buffer.from(JSON.stringify({format:1,kind:'wasm-posix-core-release',files},null,2)+'\n');
await writeFile(join(out,'release.json'),bytes);
console.log(createHash('sha256').update(bytes).digest('hex'));
