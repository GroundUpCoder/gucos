import {gunzipSync} from 'node:zlib';
import {safePath} from './patch.mjs';
// Source import only. Links/special files are refused, never followed on the host.
export function unpackSource(bytes,{gzip=false,strip=0}={}){
 if(!Number.isSafeInteger(strip)||strip<0)throw Error('Invalid strip count');
 if(gzip)bytes=gunzipSync(bytes,{maxOutputLength:256*1024*1024});
 const files=new Map();let at=0,pendingName=null;
 const text=b=>b.toString('utf8').split('\0')[0];
 const number=b=>{const s=text(b).trim();if(!/^[0-7]+$/.test(s))throw Error('Unsupported tar number');return parseInt(s,8);};
 while(at+512<=bytes.length){
  const h=bytes.subarray(at,at+512);at+=512;if(h.every(x=>x===0)){if(!bytes.subarray(at).every(x=>x===0))throw Error('Trailing tar data');return files;}
  const sum=h.reduce((n,x,i)=>n+(i>=148&&i<156?32:x),0);if(sum!==number(h.subarray(148,156)))throw Error('Tar header checksum mismatch');
  const size=number(h.subarray(124,136)),type=h[156],end=at+size;if(end>bytes.length)throw Error('Truncated tar');
  const data=bytes.subarray(at,end);at+=Math.ceil(size/512)*512;
  if(type===76){pendingName=text(data).replace(/\n$/,'');continue;}
  if(type===120||type===103)throw Error('PAX source archives are not supported; provide a USTAR archive or explicit source files');
  let name=pendingName??[text(h.subarray(345,500)),text(h.subarray(0,100))].filter(Boolean).join('/');pendingName=null;
  name=name.replace(/^\.\//,'').replace(/\/$/,'');
  if(!safePath(name))throw Error('Unsafe tar path: '+name);
  if(type===53)continue;
  if(type!==0&&type!==48)throw Error('Source tar contains a link or special file: '+name);
  const path=name.split('/').slice(strip).join('/');if(!path)continue;
  if(!safePath(path)||files.has(path))throw Error('Duplicate/unsafe tar destination');files.set(path,Buffer.from(data));
 }
 throw Error('Missing tar terminator');
}
