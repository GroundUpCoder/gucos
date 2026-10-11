// Node-side provider. The compiler itself has no filesystem dependency.
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
export function libcDigest(manifest,sources,headers) {
  const {digest,preludeText,...metadata}=manifest;
  const hash=createHash('sha256').update(JSON.stringify(metadata)+'\n');
  for(const [dir,names,entries] of [['src',manifest.sources,sources],['include',manifest.headers,headers]])
    for(const name of names)hash.update(dir+'/'+name+'\0').update(entries[name]).update('\0');
  hash.update(manifest.prelude+'\0').update(manifest.preludeText).update('\0');
  return hash.digest('hex');
}
export function loadLibc(root=new URL('../libc/',import.meta.url)) {
  const manifest=JSON.parse(readFileSync(new URL('manifest.json',root),'utf8'));
  if(manifest.format!==1)throw Error('Unsupported libc manifest');
  const readText=name=>{
    if(!/^[A-Za-z0-9_.-]+(?:\/[A-Za-z0-9_.-]+)*$/.test(name)||name.split('/').some(p=>p==='.'||p==='..'))throw Error('Invalid libc file name: '+name);
    return readFileSync(new URL(name,root),'utf8');
  };
  const read=(dir,names)=>Object.fromEntries(names.map(name=>[name,readText(dir+'/'+name)]));
  const sources=read('src',manifest.sources),headers=read('include',manifest.headers);
  manifest.preludeText=readText(manifest.prelude);
  const digest=libcDigest(manifest,sources,headers);
  if(manifest.digest!==digest)throw Error('Stale libc digest; run node tools/libc-manifest.mjs');
  return {sources,headers,manifest};
}
export function installLibc(compiler,provider=loadLibc()) {
  compiler.setStdlibSources(provider.sources,provider.headers,provider.manifest);
  return provider;
}
export function libcModule(provider=loadLibc()) {
  return 'self.CompilerJS.setStdlibSources('+[provider.sources,provider.headers,provider.manifest].map(JSON.stringify).join(',')+');\n';
}
