
import assert from 'node:assert/strict';
import {readFile,writeFile,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {browserSession} from '/home/ubuntu/git/gucos-ipc-final/tools/browser.mjs';
const root='/home/ubuntu/git/gucos-ipc-final', out=fileURLToPath(new URL('.',import.meta.url));
const expected='be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb';
const retained=root+'/docs/research/platform-1.0/retained-artifacts';
const sha=b=>createHash('sha256').update(b).digest('hex');
const save=(name,data)=>writeFile(out+name,JSON.stringify(data,null,2)+'\n');
async function snapshot(){
 const hashes={};
 async function walk(dir){for(const item of await readdir(root+'/'+dir,{withFileTypes:true})){
 const p=dir+'/'+item.name;if(item.isDirectory())await walk(p);else if(item.isFile())hashes[p]=sha(await readFile(root+'/'+p));
 }}
 await walk('dist');
 return {runtime:JSON.parse(await readFile(root+'/dist/build.json')).runtime,hashes,
 head:execFileSync('git',['-C',root,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),
 status:execFileSync('git',['-C',root,'status','--short'],{encoding:'utf8'})};
}
const result={scope:'retained-artifact ABI; no old-disk migration or published-package coverage',startedUTC:new Date().toISOString(),status:'failed',checks:[]};
let s,before;
async function command(name,script,marker){
 const log=await s.command(script);await writeFile(out+name+'.log',log);console.log(name+'\n'+log);
 if(marker)assert.match(log,marker);result.checks.push(name);await save('results.json',result);return log;
}
async function verifyGuest(name){
 const hashes={};
 for(const [file,expected]of Object.entries(result.provenance.hashes)){
 const bytes=Buffer.from(await s.page.evaluate(async path=>Array.from(await shell.busybox.read(path)),'/home/user/abi/'+file));
 hashes[file]=sha(bytes);assert.equal(hashes[file],expected,file);
 }
 await save(name+'.json',hashes);result.checks.push(name);console.log(name+' PASS');
}
try{
 before=await snapshot();await save('before.json',before);assert.equal(before.runtime,expected);
 await writeFile(out+'build.json',await readFile(root+'/dist/build.json'));
 result.provenance=JSON.parse(await readFile(retained+'/provenance.json'));
 assert.equal(result.provenance.baseline,'2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f');
 await save('provenance.json',result.provenance);
 const artifacts={};
 for(const [name,hash]of Object.entries(result.provenance.hashes)){
 const bytes=await readFile(retained+'/'+name);assert.equal(sha(bytes),hash,name);artifacts[name]=bytes;
 }
 result.checks.push('host provenance hashes');console.log('HOST-PROVENANCE-PASS');
 s=await browserSession({directory:"/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7",timeout:180000});
 result.servedIdentity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));
 assert.equal(result.servedIdentity.runtime,expected);
 await command('prepare','set -eu\nmkdir -p /home/user/abi');
 for(const [name,bytes]of Object.entries(artifacts))await s.page.evaluate(async({path,bytes})=>shell.busybox.write(path,new Uint8Array(bytes)),{path:'/home/user/abi/'+name,bytes:Array.from(bytes)});
 await verifyGuest('installed-hashes');
 await command('unchanged-old-app','set -eu\nchmod +x /home/user/abi/old-app /home/user/abi/old-dynamic\n/home/user/abi/old-app',/OLD-ANSWER-42/);
 await command('unchanged-old-dynamic','/home/user/abi/old-dynamic',/OLD-DSO-42/);
 const source=result.provenance.sources['/tmp/consumer.c'];assert.equal(typeof source,'string');
 await writeFile(out+'consumer.c',source);
 await s.page.evaluate(async source=>shell.busybox.write('/home/user/abi/consumer.c',new TextEncoder().encode(source)),source);
 await command('relink-old-archive','set -eu\ncc /home/user/abi/consumer.c /home/user/abi/libanswer.a -o /home/user/abi/relinked-archive\n/home/user/abi/relinked-archive',/OLD-ANSWER-42/);
 await command('relink-old-object','set -eu\ncc /home/user/abi/consumer.c /home/user/abi/answer.o -o /home/user/abi/relinked-object\n/home/user/abi/relinked-object',/OLD-ANSWER-42/);
 await verifyGuest('post-run-hashes');
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 result.reloadIdentity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));assert.equal(result.reloadIdentity.runtime,expected);
 await command('reload-old-dynamic','/home/user/abi/old-dynamic',/OLD-DSO-42/);
 await verifyGuest('reload-hashes');
 for(const [name,hash]of Object.entries(result.provenance.hashes))assert.equal(sha(await readFile(retained+'/'+name)),hash,name);
 result.status='passed';console.log('RETAINED-ARTIFACT-ABI-PASS');
}catch(error){result.error=String(error.stack||error);process.exitCode=1;console.error(result.error);}
finally{
 if(s)await s.close();
 const after=await snapshot();await save('after.json',after);
 result.distUnchanged=JSON.stringify(before?.hashes)===JSON.stringify(after.hashes);
 result.headUnchanged=before?.head===after.head;result.statusUnchanged=before?.status===after.status;
 if(!result.distUnchanged){result.status='failed';process.exitCode=1;}
 result.completedUTC=new Date().toISOString();await save('results.json',result);
}
