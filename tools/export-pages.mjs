// Prepare a fresh Pages directory from the exact, verified core asset closure.
import {readFile,writeFile,mkdir,copyFile} from 'node:fs/promises';
import {resolve,join,dirname} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const [input='dist',output]=process.argv.slice(2);
if(!output)throw Error('usage: node tools/export-pages.mjs DIST NEW_RELEASE_DIRECTORY');
const out=resolve(output);await mkdir(dirname(out),{recursive:true});
const result=spawnSync(process.execPath,[new URL('export-release.mjs',import.meta.url).pathname,resolve(input),out],{stdio:'inherit'});
if(result.status!==0)process.exit(result.status||1);
const manifest=JSON.parse(await readFile(join(out,'release.json')));
for(const [path,entry] of Object.entries(manifest.files))if(entry.size>25*1024*1024)throw Error('Pages asset exceeds 25 MiB: '+path);
// Keep release metadata outside the uploaded public directory.
const site=join(out,'site');await mkdir(join(site,'assets'),{recursive:true});
for(const path of Object.keys(manifest.files))await copyFile(join(out,path),join(site,path));
await writeFile(join(site,'_headers'),`/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
  Cross-Origin-Resource-Policy: same-origin
  X-Content-Type-Options: nosniff
  Cache-Control: no-cache

/assets/*
  ! Cache-Control
  Cache-Control: public, max-age=31536000, immutable
`);
// Disable Pages' implicit SPA fallback: missing objects must return 404.
await writeFile(join(site,'404.html'),'<!doctype html><meta charset="utf-8"><title>Not found</title><h1>Not found</h1>\n');
const uploadFiles={...manifest.files};
for(const path of ['_headers','404.html']){
 const b=await readFile(join(site,path));uploadFiles[path]={size:b.length,sha256:createHash('sha256').update(b).digest('hex')};
}
await writeFile(join(out,'pages-release.json'),JSON.stringify({format:1,files:uploadFiles},null,2)+'\n');
console.log('Pages upload directory: '+site);
