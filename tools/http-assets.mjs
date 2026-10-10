import {readFile,writeFile,stat,rename} from 'node:fs/promises';
import {gzip} from 'node:zlib';
import {promisify} from 'node:util';

const compress=promisify(gzip);
const sidecar=file=>file instanceof URL?new URL(file.href+'.gz'):file+'.gz';

// Generate once at publication time. Content hashes still identify the
// uncompressed bytes; HTTP decoding preserves package integrity checks.
export async function ensureGzip(file) {
  const source=await stat(file), target=sidecar(file);
  const cached=await stat(target).catch(()=>null);
  if(cached && cached.mtimeMs>=source.mtimeMs)return cached.size;
  const bytes=await compress(await readFile(file),{level:6});
  const temporary=target instanceof URL?new URL(target.href+'.tmp-'+process.pid):target+'.tmp-'+process.pid;
  await writeFile(temporary,bytes);
  await rename(temporary,target);
  return bytes.length;
}

export function acceptsGzip(header='') {
  const encodings=new Map(String(header).toLowerCase().split(',').map(part=>{
    const [name,...parameters]=part.trim().split(';');
    const q=parameters.find(p=>p.trim().startsWith('q='));
    return [name,q===undefined?1:Number(q.trim().slice(2))];
  }));
  return (encodings.get('gzip')??encodings.get('*')??0)>0;
}

export async function serveAsset(req,res,file,headers={}) {
  let bytes,compressed=false;
  if(acceptsGzip(req.headers['accept-encoding'])) {
    bytes=await readFile(sidecar(file)).catch(error=>{if(error.code==='ENOENT')return null;throw error;});
    compressed=bytes!==null;
  }
  if(!bytes)bytes=await readFile(file);
  res.writeHead(200,{...headers,Vary:'Accept-Encoding','Content-Length':bytes.length,...(compressed?{'Content-Encoding':'gzip'}:{})});
  res.end(req.method==='HEAD'?undefined:bytes);
}
