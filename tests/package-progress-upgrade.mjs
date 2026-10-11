import http from 'node:http';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {browserSession} from '../tools/browser.mjs';
let root=process.env.PROGRESS_BASELINE||'/tmp/wasm-posix-platform-c4411d72';
const old=JSON.parse(await readFile(resolve(root,'build.json')));
assert.equal(old.platform.version,'1.0.0');
const candidate=resolve('dist'),next=JSON.parse(await readFile(resolve(candidate,'build.json')));
const server=http.createServer(async(req,res)=>{try{
 const p=new URL(req.url,'http://localhost').pathname;
 const data=await readFile(resolve(root,p==='/'?'index.html':p.slice(1)));
 res.writeHead(200,{'Content-Type':p==='/'?'text/html':p.endsWith('.js')?'text/javascript':'application/octet-stream','Cross-Origin-Opener-Policy':'same-origin','Cross-Origin-Embedder-Policy':'require-corp'});res.end(data);
}catch{res.writeHead(404);res.end('missing');}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));const url='http://127.0.0.1:'+server.address().port;
let s;
try{
 s=await browserSession({url});
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),old.runtime);
 const before=await s.command('sha256sum /usr/libexec/guc-repo-fetch');
 await s.command('echo preserved > /home/user/progress-upgrade-marker\nsync');
 root=candidate;await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),next.runtime);
 const after=await s.command('sha256sum /usr/libexec/guc-repo-fetch');assert.notEqual(before,after);
 const log=await s.command(`set -eu\ngrep -Fx preserved /home/user/progress-upgrade-marker\nif /usr/libexec/guc-repo-fetch -p 0 ${url}/missing /tmp/progress-missing; then exit 99; fi\ngucman platform`);
 assert.match(log,/Connecting; waiting for response/);assert.match(log,/HTTP 404/);assert.match(log,/1.0.1/);
 await writeFile('docs/research/package-progress-1.0.1/upgrade.json',JSON.stringify({status:'passed',beforeRuntime:old.runtime,afterRuntime:next.runtime,before,after,log},null,2));
 console.log('PASS existing 1.0.0 disk receives new private fetch helper and platform; user file preserved');
}finally{if(s)await s.close();await new Promise(r=>server.close(r));}
