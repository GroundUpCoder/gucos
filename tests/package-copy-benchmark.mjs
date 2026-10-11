// Same immutable 529-file archive, fresh browser disks, serial old/new runs.
import assert from 'node:assert/strict';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {browserSession} from '../tools/browser.mjs';
import {sourceTar} from '../tools/hosted-assets.mjs';
assert(process.env.MACHINE_JOB_ID);
const [beforeDir,output]=process.argv.slice(2);assert(beforeDir&&output);
const sha=b=>createHash('sha256').update(b).digest('hex');
const entries=[['bin/copy-probe',Buffer.from('#!/bin/sh\necho COPY-PROBE-PASS\n'),493],['share/licenses/copy-probe/LICENSE',Buffer.from('Public domain test fixture\n'),420]];
for(let i=0;i<527;i++)entries.push(['include/copy-probe/file-'+String(i).padStart(4,'0')+'.h',Buffer.alloc(4096,i%256),420]);
const archive=sourceTar(entries);let url;
function manifest(){return {format:1,name:'copy-probe',version:'1.0.0',dependencies:[],dependencyRequirements:{},platform:{target:'wasm32-wasm-posix',minVersion:'1.0.0',runtimeAbi:5,features:['posix-core']},licenses:['share/licenses/copy-probe/LICENSE'],outputs:entries.map(([path,,mode])=>({path,mode})),binary:{url:url+'/archive.tar',sha256:sha(archive),size:archive.length}};}
const server=http.createServer((req,res)=>{
 res.setHeader('Access-Control-Allow-Origin','*');
 if(req.url==='/archive.tar')res.end(archive);
 else if(req.url==='/manifest.json')res.end(JSON.stringify(manifest()));
 else if(req.url==='/catalog.json'){const b=Buffer.from(JSON.stringify(manifest()));res.end(JSON.stringify({format:1,packages:{'copy-probe':{url:url+'/manifest.json',sha256:sha(b),size:b.length}}}));}
 else{res.statusCode=404;res.end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));url='http://127.0.0.1:'+server.address().port;
const result={archive:{sha256:sha(archive),size:archive.length,files:entries.length},scope:'Synthetic header-heavy archive. One fresh-disk measurement per runtime; phase timestamps sampled every 10 ms, not a statistical performance guarantee.',runs:[]};
try{
 for(const [label,directory] of [['before',resolve(beforeDir)],['after',resolve('dist')]]){
  const build=JSON.parse(await readFile(resolve(directory,'build.json')));
  const s=await browserSession({directory,allowedOrigins:[url]});
  try{
   await s.command('gucman repo remove groundupcoder\ngucman repo add probe '+url+'/catalog.json');
   await s.page.evaluate(()=>{window.copyTimes={start:performance.now(),marks:{}};window.copyTimer=setInterval(()=>{for(const marker of ['Installing copy-probe: 0/529 files','Preparing checksums: copy-probe','Saving transaction: copy-probe','Activating: copy-probe'])if(!copyTimes.marks[marker]&&shell.busybox.log.includes(marker))copyTimes.marks[marker]=performance.now();},10);});
   const start=performance.now();const log=await s.command('gucman install copy-probe');const totalMs=performance.now()-start;
   const marks=await s.page.evaluate(()=>{clearInterval(copyTimer);return copyTimes.marks;});
   assert.match(log,/Installing copy-probe: 529\/529 files/);assert.match(log,/Checksummed copy-probe: 529\/529 files/);
   assert.match(await s.command('copy-probe'),/COPY-PROBE-PASS/);
   const sums=entries.map(([path,b])=>sha(b)+'  /usr/local/'+path).join('\n')+'\n';
   await s.page.evaluate(async text=>shell.busybox.write('/tmp/probe.sha256',new TextEncoder().encode(text)),sums);
   await s.command('sha256sum -c /tmp/probe.sha256');
   result.runs.push({label,runtime:build.runtime,totalMs,copyMs:marks['Preparing checksums: copy-probe']-marks['Installing copy-probe: 0/529 files'],checksumMs:marks['Saving transaction: copy-probe']-marks['Preparing checksums: copy-probe'],marks,log});
   console.log(label,JSON.stringify({...result.runs.at(-1),log:undefined}));
  }finally{await s.close();}
 }
 result.status='passed';
}finally{await new Promise(r=>server.close(r));await writeFile(output,JSON.stringify(result,null,2)+'\n',{flag:'wx'});}
