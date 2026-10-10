import {mkdir,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {ensureGzip} from './http-assets.mjs';

// Deterministic USTAR archives: binary contents never pass through JSON/base64.
export function sourceTar(entries) {
  const parts=[];
  for(const [path,bytes,mode=0o644] of entries){
    const h=Buffer.alloc(512);let name=path,prefix='';
    if(Buffer.byteLength(name)>100){const i=path.lastIndexOf('/',155);prefix=path.slice(0,i);name=path.slice(i+1);}
    if(Buffer.byteLength(name)>100||Buffer.byteLength(prefix)>155)throw Error('USTAR path too long: '+path);
    h.write(name,0,100);h.write(mode.toString(8).padStart(7,'0')+'\0',100);h.write('0000000\0',108);h.write('0000000\0',116);
    h.write(bytes.length.toString(8).padStart(11,'0')+'\0',124);h.write('00000000000\0',136);
    h.fill(32,148,156);h[156]=48;h.write('ustar\0',257);h.write('00',263);h.write(prefix,345,155);
    h.write([...h].reduce((a,b)=>a+b,0).toString(8).padStart(6,'0')+'\0 ',148);
    parts.push(h,bytes,Buffer.alloc((512-bytes.length%512)%512));
  }
  return Buffer.concat([...parts,Buffer.alloc(1024)]);
}
export async function hostedPayload(payload,root){
  await mkdir(new URL('assets/',root),{recursive:true});
  const assetURLs=new Set(),identities={};
  const asset=async(bytes,ext)=>{
    const hash=createHash('sha256').update(bytes).digest('hex'),url=`assets/${hash}.${ext}`;
    const file=new URL(url,root);
    await writeFile(file,bytes,{flag:'wx'}).catch(error=>{if(error.code!=='EEXIST')throw error;});
    await ensureGzip(file);assetURLs.add(url);identities[url]={sha256:hash,size:bytes.length};return url;
  };
  const p={...payload,hosted:true,files:{},binaryFiles:{},prebuilt:{},sourceArchives:{},dataFiles:{}};
  const trees=new Map();
  for(const [path,value] of [...Object.entries(payload.files),...Object.entries(payload.binaryFiles)]){
    const bytes=Object.hasOwn(payload.binaryFiles,path)?Buffer.from(value,'base64'):Buffer.from(value);
    const match=/^\/usr\/src\/([^/]+)\//.exec(path);
    // These legacy ports look up runtime data beneath /usr/src. Ship the
    // data independently of their build sources.
    if((payload.distributionDataPaths||[]).some(p=>p.endsWith('/')?path.startsWith(p):path===p))p.dataFiles[path]=await asset(bytes,'bin');
    if(match){if(!trees.has(match[1]))trees.set(match[1],[]);trees.get(match[1]).push([path.slice(9),bytes]);}
    else p.files[path]=value;
  }
  const notices=payload.licenseNotices;
  if(notices)for(const [tree,entries] of trees)if(notices.trees[tree])entries.push([tree+'/.gucman-licenses.txt',Buffer.from(notices.trees[tree])]);
  for(const [tree,entries] of trees)p.sourceArchives[tree]=await asset(sourceTar(entries),'tar');
  const escape=text=>text.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('"','&quot;');
  // Repository boot omits the legacy per-tree lookup table. Keep a separate
  // static source offer, including every emitted archive, reachable from notices.
  if(payload.repository){
    const rows=Object.entries(p.sourceArchives).sort(([a],[b])=>a<b?-1:a>b?1:0).map(([tree,url])=>'<li><a href="'+url.slice('assets/'.length)+'">'+escape(tree)+'</a> — '+identities[url].size+' bytes; SHA256 '+identities[url].sha256+'</li>').join('\n');
    p.sourceIndexURL=await asset(Buffer.from('<!doctype html><meta charset="utf-8"><title>Source archives</title><h1>Source archives</h1><p>Source trees emitted by this OS build. Package source references and licenses are recorded in their remote manifests (gucman info SOURCE:NAME). This list does not assert package acceptance or runtime compatibility.</p><ul>'+rows+'</ul>'),'html');
  }
  if(notices){
    p.platformLicenses={};p.packageLicenses={};
    for(const [name,text] of Object.entries(notices.platform))p.platformLicenses[name]=await asset(Buffer.from(text),'txt');
    for(const [name,text] of Object.entries(notices.packages))p.packageLicenses[name]=await asset(Buffer.from(text),'txt');
    const links=entries=>Object.entries(entries).map(([name,url])=>'<li><a href="'+url.slice('assets/'.length)+'">'+escape(name)+'</a></li>').join('\n');
    const sourceLink=p.sourceIndexURL?'<p><a href="'+p.sourceIndexURL.slice('assets/'.length)+'">Source archives</a></p>':'';
    p.licensesURL=await asset(Buffer.from('<!doctype html><meta charset="utf-8"><title>License notices</title><h1>License notices</h1>'+sourceLink+'<h2>Platform</h2><ul>'+links(p.platformLicenses)+'</ul><h2>Packages</h2><ul>'+links(p.packageLicenses)+'</ul>'),'html');
    delete p.licenseNotices;
  }
  delete p.licenses;delete p.distributionDataPaths;
  for(const [path,e] of Object.entries(payload.prebuilt))p.prebuilt[path]={key:e.key,url:await asset(Buffer.from(e.bytes,'base64'),'bin')};
  for(const key of ['systemOwner','systemProcess','systemCompiler','worker']){p[key+'URL']=await asset(Buffer.from(payload[key]),'js');delete p[key];}
  for(const [key,path] of Object.entries(payload.distributionAssetAliases||{})){p[key+'URL']=await asset(Buffer.from(payload[key],'base64'),'bin');p.dataFiles[path]=p[key+'URL'];p[key]='';}
  delete p.distributionAssetAliases;
  // Sources are explicitly provisioned, preserving existing files. Never carry
  // old embedded-source migration payloads into the page.
  for(const key of Object.keys(p))if(key.endsWith('Migration'))p[key]={};
  p.assetURLs=[...assetURLs].sort();
  if(payload.repository){
    // Build-side inventory only. Every listed asset remains required for complete
    // distribution; partitioning does not authorize source omission or a route.
    const sourceURLs=new Set(Object.values(p.sourceArchives));
    await writeFile(new URL('hosted-distribution.json',root),JSON.stringify({format:1,scope:'Complete repository-mode emitted assets; transport and release approval required',sources:p.sourceArchives,sourceIndexURL:p.sourceIndexURL,licensesURL:p.licensesURL,assets:Object.fromEntries(p.assetURLs.map(url=>[url,{...identities[url],role:sourceURLs.has(url)?'source-archive':'runtime-or-notice'}]))},null,2)+'\n');
  }
  return p;
}
