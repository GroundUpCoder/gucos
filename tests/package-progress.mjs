import assert from 'node:assert/strict';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
import {sourceTar} from '../tools/hosted-assets.mjs';
const runtime=JSON.parse(await readFile(new URL('../dist/build.json',import.meta.url))).runtime;
const archive=sourceTar([['bin/progress-demo',Buffer.from('#!/bin/sh\necho PROGRESS-DEMO-PASS\n'),493],['share/licenses/progress-demo/LICENSE',Buffer.from('Public domain\n')],['share/progress-demo/data',Buffer.alloc(262144,65)]]);
const sha=b=>createHash('sha256').update(b).digest('hex');
let release,started,version='1',bad=false,slow=true;
const held=new Promise(r=>release=r),requested=new Promise(r=>started=r);
let url;
const server=http.createServer(async(req,res)=>{
 res.setHeader('Access-Control-Allow-Origin','*');
 if(req.url==='/archive.tar'){
  res.writeHead(200,{'Content-Length':archive.length});
  if(slow){res.write(archive.subarray(0,65536));await new Promise(r=>setTimeout(r,700));res.write(archive.subarray(65536,131072));started();await held;res.end(archive.subarray(131072));}
  else res.end(archive);
 }else if(req.url==='/catalog.json'){
  const bytes=Buffer.from(JSON.stringify(manifest()));
  res.end(JSON.stringify({format:1,packages:{'progress-demo':{url:url+'/manifest.json',sha256:sha(bytes),size:bytes.length}}}));
 }else if(req.url==='/manifest.json')res.end(JSON.stringify(manifest()));
 else {res.statusCode=404;res.end('missing');}
});
function manifest(){return {format:1,name:'progress-demo',version,dependencies:[],runtime,licenses:['share/licenses/progress-demo/LICENSE'],outputs:[{path:'bin/progress-demo',mode:493},{path:'share/licenses/progress-demo/LICENSE',mode:420},{path:'share/progress-demo/data',mode:420}],binary:{url:url+'/archive.tar',size:archive.length,sha256:bad?'0'.repeat(64):sha(archive)}};}
await new Promise(r=>server.listen(0,'127.0.0.1',r));url='http://127.0.0.1:'+server.address().port;
let s, heartbeat;
try{
 s=await browserSession({allowedOrigins:[url]});
 heartbeat=setInterval(async()=>{try{console.log(await s.page.evaluate(()=>shell.busybox?.log.slice(-1600)));}catch{}},15000);
 await s.command(`gucman repo remove groundupcoder\ngucman repo add progress ${url}/catalog.json`);
 const pending=s.command('gucman install progress-demo');
 // No stdout path contamination: manifest and archive are captured by shell substitutions.
 await Promise.race([requested,pending.then(()=>{throw Error('Install completed before held download');})]);
 await s.page.waitForFunction(size=>new RegExp('Downloaded [0-9]+ / '+size+' bytes').test(shell.busybox.log),archive.length);
 const during=await s.page.evaluate(()=>shell.busybox.log);
 assert.match(during,/install progress-demo: reading catalogs/);
 assert.match(during,/Package 1\/1: progress-demo \(0 remaining after this\)/);
 assert.doesNotMatch(during,/Installed progress:progress-demo/);
 assert.doesNotMatch(during.slice(during.lastIndexOf('Downloading progress-demo archive')),/100%|transfer finished/);
 release();const log=await pending;
 for(const message of ['transfer finished','Unpacking: progress-demo','Installing progress-demo: 3/3 files','Checksummed progress-demo: 3/3 files','Verifying staged files: progress-demo','Activating: progress-demo','Installed progress:progress-demo'])assert.ok(log.includes(message),message);
 assert.match(await s.command('progress-demo'),/PROGRESS-DEMO-PASS/);
 assert.doesNotMatch(log,/: OK/);
 slow=false;version='2';
 const cached=await s.command('gucman upgrade progress-demo');
 assert.match(cached,/Checking cached object: progress-demo archive/);
 assert.doesNotMatch(cached,/Downloading progress-demo archive/);
 version='3';
 const object='/var/cache/gucman/v1/objects/'+sha(archive);
 await s.command(`set -eu\nmv '${object}' /tmp/progress-cached-archive\nmkdir '${object}'`);
 await assert.rejects(()=>s.command('gucman upgrade progress-demo'),/nonregular cache object/);
 await s.command(`set -eu\ntest -z "$(ls -A '${object}')"\nrmdir '${object}'\nmkdir /tmp/progress-cache-directory\nln -s /tmp/progress-cache-directory '${object}'`);
 await assert.rejects(()=>s.command('gucman upgrade progress-demo'),/symlink cache object/);
 await s.command(`set -eu\ntest -z "$(ls -A /tmp/progress-cache-directory)"\nrm '${object}'\nmv /tmp/progress-cached-archive '${object}'`);
 version='4';bad=true;
 await assert.rejects(()=>s.command('gucman upgrade progress-demo'),/object checksum mismatch/);
 assert.match(await s.command('progress-demo'),/PROGRESS-DEMO-PASS/);
 await s.command(`set -eu
state=/usr/local/lib/gucman
release=$(readlink "$state/packages/progress-demo/current")
mkdir "$state/pending"
printf 'progress-demo\\n' > "$state/pending/name"
printf '%s\\n' "$release" > "$state/pending/release"
printf '%064d  %s/bin/progress-demo\\n' 0 "$release" > "$state/pending/checksums"
touch "$state/pending/ready"`);
 await assert.rejects(()=>s.command('gucman recover'),/progress-demo: FAILED[\s\S]*pending release metadata changed/);
 assert.match(await s.command('progress-demo'),/PROGRESS-DEMO-PASS/);
 await s.command('rm -rf /usr/local/lib/gucman/pending');
 const failure=await s.command(`if guc-fetch -p 0 ${url}/missing /tmp/missing; then exit 99; fi`);
 assert.match(failure,/HTTP 404/);assert.doesNotMatch(failure,/transfer finished/);
 console.log('PASS incremental bytes before completion, immediate phases, counts, cached transfer, checksum failure preserving install, HTTP failure');
}finally{clearInterval(heartbeat);release();if(s)await s.close();await new Promise(r=>server.close(r));}
