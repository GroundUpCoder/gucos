import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {serve} from '../tools/serve.mjs';
import {browserSession} from '../tools/browser.mjs';
const baseline=process.env.BASELINE_CORE||'releases/terminal-echo-1.0.2-final';
const oldBuild=JSON.parse(await readFile(baseline+'/build.json'));
assert.equal(oldBuild.platform.version,'1.0.2');
const next=JSON.parse(await readFile('dist/build.json'));
const old=await serve({directory:baseline}),candidate=await serve();
const s=await browserSession({url:old.url});
try{
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),oldBuild.runtime);
 await s.command('mkdir -p /home/user/abi');
 const root='docs/research/platform-1.0/retained-artifacts';
 const p=JSON.parse(await readFile(root+'/provenance.json'));
 for(const [name,hash]of Object.entries(p.hashes)){
  const bytes=await readFile(root+'/'+name);assert.equal(createHash('sha256').update(bytes).digest('hex'),hash);
  await s.page.evaluate(({name,bytes})=>shell.busybox.write('/home/user/abi/'+name,new Uint8Array(bytes)),{name,bytes:Array.from(bytes)});
 }
 await s.page.evaluate(()=>shell.busybox.write('/home/user/abi/consumer.c',new TextEncoder().encode('#include <stdio.h>\nint answer(void);int main(void){printf("OLD-ANSWER-%d\\n",answer());return answer()!=42;}\n')));
 console.log(await s.command('set -eu\nchmod +x /home/user/abi/old-app /home/user/abi/old-dynamic\n/home/user/abi/old-app\n/home/user/abi/old-dynamic\necho keep-me > /home/user/patch-marker\nsync'));
 old.server.removeAllListeners('request');old.server.on('request',candidate.server.listeners('request')[0]);
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),next.runtime);
 console.log(await s.command('set -eu\ngrep -Fx keep-me /home/user/patch-marker\n/home/user/abi/old-app\n/home/user/abi/old-dynamic\ncc /home/user/abi/consumer.c /home/user/abi/libanswer.a -o /tmp/relinked\n/tmp/relinked\ncc /home/user/abi/consumer.c /home/user/abi/answer.o -o /tmp/relinked-object\n/tmp/relinked-object\ngucman platform'));
 for(const [name,hash]of Object.entries(p.hashes))assert.match(await s.command('sha256sum /home/user/abi/'+name),new RegExp(hash));
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('/home/user/abi/old-dynamic'),/OLD-DSO-42/);
 await writeFile('docs/research/agent-vt-1.0.3/upgrade.json',JSON.stringify({status:'passed',baseline:oldBuild.runtime,runtime:next.runtime,retainedArtifactProvenance:p,checks:['existing 1.0.2 disk upgrade','user marker preserved','unchanged executable and DSO run','unchanged object/archive relink','artifact hashes preserved','second reload']},null,2)+'\n');
 console.log('PASS 1.0.2 disk upgrade, retained ABI and reload');
}finally{await s.close();await old.close();await candidate.close();}
