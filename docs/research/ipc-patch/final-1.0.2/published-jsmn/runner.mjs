
import assert from 'node:assert/strict';
import {readFile,writeFile,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {browserSession} from '/home/ubuntu/git/gucos-ipc-final/tools/browser.mjs';
const root='/home/ubuntu/git/gucos-ipc-final',out=fileURLToPath(new URL('.',import.meta.url));
const expected='be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb';
const sha=b=>createHash('sha256').update(b).digest('hex');
const save=(name,data)=>writeFile(out+name,JSON.stringify(data,null,2)+'\n');
async function snapshot(){
 const hashes={};async function walk(dir){for(const item of await readdir(root+'/'+dir,{withFileTypes:true})){
 const p=dir+'/'+item.name;if(item.isDirectory())await walk(p);else if(item.isFile())hashes[p]=sha(await readFile(root+'/'+p));
 }}await walk('dist');
 return {hashes,runtime:JSON.parse(await readFile(root+'/dist/build.json')).runtime,
 head:execFileSync('git',['-C',root,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),
 status:execFileSync('git',['-C',root,'status','--short'],{encoding:'utf8'})};
}
const result={status:'failed',startedUTC:new Date().toISOString(),operation:'gucman install groundupcoder:jsmn',scope:'published binary install and execution; no source build or GPU test',network:[]};
let s,before;
async function command(name,script){
 try {const log=await s.command(script);await writeFile(out+name+'.log',log);console.log(name+'\n'+log);return log;}
 catch(e){await writeFile(out+name+'.log',String(e.stack||e));throw e;}
}
async function identity(){
 const value=await s.page.evaluate(async()=>({build:await fetch('/build.json').then(r=>r.json()),systemBuild:shell.toolchain.systemBuild}));
 assert.equal(value.build.runtime,expected);assert.equal(value.systemBuild,expected);return value;
}
async function artifact(name){
 const bytes=Buffer.from(await s.page.evaluate(async()=>Array.from(await shell.busybox.read('/usr/local/bin/jsmn-example'))));
 await writeFile(out+name+'.wasm',bytes);return {path:'/usr/local/bin/jsmn-example',size:bytes.length,sha256:sha(bytes)};
}
try{
 before=await snapshot();await save('before.json',before);assert.equal(before.runtime,expected);
 s=await browserSession({directory:"/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7",allowedOrigins:['https://raw.githubusercontent.com'],timeout:180000});
 // A new browserSession creates a new Chromium process and nonpersistent context.
 result.identity=await identity();
 await s.page.context().route('https://raw.githubusercontent.com/**',async route=>{
  const row={index:result.network.length,url:route.request().url(),method:route.request().method(),startedUTC:new Date().toISOString()};
  result.network.push(row);
  try{
   assert.equal(row.method,'GET');
   const response=await route.fetch({maxRedirects:0}),body=await response.body();
   row.status=response.status();row.responseURL=response.url();row.sha256=sha(body);row.size=body.length;
   row.file='response-'+String(row.index).padStart(3,'0')+'.bin';
   const h=response.headers();row.headers={etag:h.etag,contentType:h['content-type'],location:h.location};
   await writeFile(out+row.file,body);await save('network.json',result.network);
   await route.fulfill({response,body});
  }catch(e){row.error=String(e);await save('network.json',result.network);await route.abort();}
 });
 assert.match(await command('repositories','gucman repo list'),/GroundUpCoder\/gucos-packages/);
 await command('fresh-profile','if test -e /usr/local/bin/jsmn-example; then echo UNEXPECTED-EXISTING-JSMN; exit 99; fi');
 await command('install','gucman install groundupcoder:jsmn');
 const first=await command('first-run','/usr/local/bin/jsmn-example');assert.match(first,/UID: 1000/);assert.match(first,/User: johndoe/);
 result.installed=await artifact('installed-jsmn-example');
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 result.reloadIdentity=await identity();
 assert.match(await command('reload-run','/usr/local/bin/jsmn-example'),/UID: 1000/);
 result.reloaded=await artifact('reloaded-jsmn-example');assert.deepEqual(result.reloaded,result.installed);
 assert(result.network.length>0,'Must retain actual public response evidence');
 result.status='passed';console.log('PUBLISHED-JSMN-BINARY-PASS');
}catch(e){result.error=String(e.stack||e);console.error(result.error);process.exitCode=1;}
finally{
 if(s)await s.close();
 const after=await snapshot();await save('after.json',after);
 result.distUnchanged=JSON.stringify(before?.hashes)===JSON.stringify(after.hashes);
 result.headUnchanged=before?.head===after.head;result.statusUnchanged=before?.status===after.status;
 if(!result.distUnchanged){result.status='failed';process.exitCode=1;}
 result.completedUTC=new Date().toISOString();await save('results.json',result);
}
