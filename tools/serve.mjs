import http from 'node:http';
import {readFile,realpath} from 'node:fs/promises';
import {resolve,sep} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
export const dist=fileURLToPath(new URL('../dist/',import.meta.url));
export async function serve({port=0,extra=new Map(),directory=dist}={}){
 const root=await realpath(directory);
 const server=http.createServer(async(req,res)=>{
  const headers={'Cross-Origin-Opener-Policy':'same-origin','Cross-Origin-Embedder-Policy':'require-corp','Cache-Control':'no-store'};
  if(!['GET','HEAD'].includes(req.method)){res.writeHead(405);res.end();return;}
  try{
   let path=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
   if(path==='/')path='/index.html';
   let bytes=extra.get(path);
   if(!bytes){const file=await realpath(resolve(root,'.'+path));if(!file.startsWith(root+sep))throw Error('outside distribution');bytes=await readFile(file);}
   const type=path.endsWith('.html')?'text/html; charset=utf-8':path.endsWith('.js')?'text/javascript':path.endsWith('.json')?'application/json':'application/octet-stream';
   res.writeHead(200,{...headers,'Content-Type':type});res.end(req.method==='HEAD'?undefined:bytes);
  }catch{res.writeHead(404,headers);res.end('Not found');}
 });
 await new Promise((ok,no)=>{server.once('error',no);server.listen(port,'127.0.0.1',ok);});
 return {server,url:'http://127.0.0.1:'+server.address().port,close:()=>new Promise(ok=>server.close(ok))};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){const s=await serve({port:Number(process.env.PORT||8080)});console.log(s.url);}
