import assert from 'node:assert/strict';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {serve} from '../tools/serve.mjs';
import {browserSession} from '../tools/browser.mjs';
const baseline=process.env.BASELINE_CORE||'/tmp/wasm-posix-core-release-2a9cca';
const old=await serve({directory:baseline}),candidate=await serve();
const s=await browserSession({url:old.url});
const evidence='docs/research/platform-1.0/retained-artifacts';
try{
 const build=JSON.parse(await readFile(baseline+'/build.json','utf8'));
 assert.equal(build.runtime,'2a9cca447a35af0d0d8894ea4b50cb254ce20821753af8ae70a1ad5060da2a0f');
 await s.command('true');
 const files={
 '/tmp/answer.c':'int answer(void){return 42;}\n',
 '/tmp/consumer.c':'#include <stdio.h>\nint answer(void);int main(void){printf("OLD-ANSWER-%d\\n",answer());return answer()!=42;}\n',
 '/tmp/dynamic.c':'#include <dlfcn.h>\n#include <stdio.h>\nint main(void){void *p=dlopen("/home/user/abi/libanswer.so",RTLD_NOW);if(!p){puts(dlerror());return 1;}int(*fn)(void)=dlsym(p,"answer");if(!fn)return 2;printf("OLD-DSO-%d\\n",fn());return fn()!=42;}\n'
 };
 for(const [path,source]of Object.entries(files))await s.page.evaluate(async({path,source})=>shell.busybox.write(path,new TextEncoder().encode(source)),{path,source});
 const provenance=JSON.parse(await readFile(evidence+'/provenance.json','utf8'));
 assert.equal(provenance.baseline,build.runtime);
 const hashes=provenance.hashes;
 await s.command('mkdir -p /home/user/abi');
 for(const [name,hash]of Object.entries(hashes)){
  const bytes=await readFile(evidence+'/'+name);assert.equal(createHash('sha256').update(bytes).digest('hex'),hash);
  await s.page.evaluate(async({path,bytes})=>shell.busybox.write(path,new Uint8Array(bytes)),{path:'/home/user/abi/'+name,bytes:Array.from(bytes)});
 }
 console.log(await s.command('set -eu\nchmod +x /home/user/abi/old-app /home/user/abi/old-dynamic\n/home/user/abi/old-app\n/home/user/abi/old-dynamic\ncp /tmp/consumer.c /home/user/abi/consumer.c'));
 old.server.removeAllListeners('request');old.server.on('request',candidate.server.listeners('request')[0]);
 // Interrupt publication after one verified runtime file was written. Metadata
 // remains old, so the next boot must refresh the complete set before execution.
 let injected=false;
 await s.page.route('**/*.js',async route=>{
  const response=await route.fetch();let body=await response.text();
  const needle='for(let i=0;i<paths.length;i++)writeFile(paths[i],bytes[i],0o644);';
  if(!injected&&body.includes(needle)){injected=true;body=body.replace(needle,'for(let i=0;i<paths.length;i++){if(i===1)throw new Error("injected runtime write failure");writeFile(paths[i],bytes[i],0o644);}');}
  await route.fulfill({response,body});
 });
 await s.page.goto(old.url+'/');
 await s.page.waitForFunction(()=>document.getElementById('startup-title')?.textContent==='Startup could not finish');
 assert.equal(injected,true);assert.match(await s.page.locator('#startup-summary').textContent(),/injected runtime write failure/);
 await s.page.unroute('**/*.js');
 await s.page.goto(old.url+'/?shell=bootstrap');await s.page.waitForFunction(()=>window.shell?.ready);

 console.log(await s.command('set -eu\n/home/user/abi/old-app\n/home/user/abi/old-dynamic\ncc /home/user/abi/consumer.c /home/user/abi/libanswer.a -o /tmp/relinked\n/tmp/relinked'));
 for(const [name,hash]of Object.entries(hashes))assert.match(await s.command('sha256sum /home/user/abi/'+name),new RegExp(hash));
 await s.page.evaluate(async source=>shell.busybox.write('/tmp/platform.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/platform-posix.c',import.meta.url),'utf8'));
 assert.match(await s.command('set -eu\ncc /tmp/platform.c -o /tmp/platform\n/tmp/platform'),/PLATFORM-POSIX-PASS/);
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('/home/user/abi/old-dynamic'),/OLD-DSO-42/);
 console.log('PASS upgraded baseline disk, unchanged executable/DSO, preserved object/archive relink, new libc APIs and second reload');
}finally{await s.close();await old.close();await candidate.close();}
