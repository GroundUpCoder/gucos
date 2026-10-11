// Refresh the digest after editing listed libc files. File order is explicit.
import fs from 'node:fs';
import {libcDigest} from '../src/libc-sources.js';
const root=new URL('../libc/',import.meta.url),url=new URL('manifest.json',root);
const manifest=JSON.parse(fs.readFileSync(url,'utf8'));
const read=(dir,names)=>Object.fromEntries(names.map(name=>[name,fs.readFileSync(new URL(dir+'/'+name,root),'utf8')]));
manifest.preludeText=fs.readFileSync(new URL(manifest.prelude,root),'utf8');
manifest.digest=libcDigest(manifest,read('src',manifest.sources),read('include',manifest.headers));
delete manifest.preludeText;
fs.writeFileSync(url,JSON.stringify(manifest,null,2)+'\n');
console.log(manifest.digest);
