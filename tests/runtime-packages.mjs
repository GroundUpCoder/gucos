import assert from 'node:assert/strict';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {browserSession,quote} from '../tools/browser.mjs';
import {sourceTar} from '../tools/hosted-assets.mjs';
const hash=b=>createHash('sha256').update(b).digest('hex');
async function repository(){
 const files=new Map();let hits=0;
 const server=http.createServer((req,res)=>{hits++;const b=files.get(req.url);res.writeHead(b?200:404,{'Access-Control-Allow-Origin':'*','Content-Type':'application/octet-stream'});res.end(b||'missing');});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const url='http://127.0.0.1:'+server.address().port;
 const put=(path,b)=>{b=Buffer.isBuffer(b)?b:Buffer.from(typeof b==='string'?b:JSON.stringify(b)+'\n');files.set(path,b);return {url:url+path,sha256:hash(b),size:b.length};};
 return {url,files,put,hits:()=>hits,close:()=>new Promise(r=>server.close(r))};
}
const a=await repository(),b=await repository();let s;
const build=JSON.parse(await readFile(new URL('../dist/build.json',import.meta.url)));
function fixture(repo,name,message,{dependencies=[],version='1',bad=false,outputs}={}){
 const inputs={
 'main.c':`#include <stdio.h>\nint main(void){puts("${message}");return 0;}\n`,
 'build.sh':`set -eu\ncc main.c -o hello\nmkdir -p "$DESTDIR/bin" "$DESTDIR/share/licenses/${name}"\ncp hello "$DESTDIR/bin/${name}"\ncp LICENSE "$DESTDIR/share/licenses/${name}/LICENSE"\n`,
 'test.sh':`set -eu\n"$DESTDIR/bin/${name}" | grep -Fx ${quote(message)}\n`,
 'LICENSE':'Test fixture; public domain.\n'};
 const files=Object.entries(inputs).map(([path,bytes])=>({path,...repo.put('/'+name+'/'+version+'/'+path,bytes)}));
 if(bad)files[0].sha256='0'.repeat(64);
 const m={format:1,name,version,dependencies,files,build:'build.sh',test:'test.sh',outputs:outputs||[{path:'bin/'+name,mode:493},{path:'share/licenses/'+name+'/LICENSE',mode:420}],licenses:['share/licenses/'+name+'/LICENSE'],runtime:build.runtime};
 return {m,ref:repo.put('/'+name+'/'+version+'/manifest.json',m)};
}
const alpha=fixture(a,'hello','HELLO-A'), beta=fixture(b,'hello','HELLO-B');
const dep=fixture(a,'dep','DEP'),consumer=fixture(a,'consumer','CONSUMER',{dependencies:['dep']});
const bad=fixture(a,'bad','BAD',{bad:true});
const cyc1=fixture(a,'cycle1','CYCLE',{dependencies:['cycle2']}),cyc2=fixture(a,'cycle2','CYCLE',{dependencies:['cycle1']});
const collision=fixture(a,'collision','COLLISION',{dependencies:['dep'],outputs:[{path:'bin/dep/child',mode:493},{path:'share/licenses/collision/LICENSE',mode:420}]});
const catalog={format:1,packages:{hello:alpha.ref,dep:dep.ref,consumer:consumer.ref,collision:collision.ref,bad:bad.ref,cycle1:cyc1.ref,cycle2:cyc2.ref}};
a.put('/catalog.json',catalog);b.put('/catalog.json',{format:1,packages:{hello:beta.ref}});
try{
 s=await browserSession({allowedOrigins:[a.url,b.url]});
 assert.equal(a.hits()+b.hits(),0,'boot must not fetch package sources');
 await s.command(`set -eu\ngucman repo remove groundupcoder\ngucman repo add a ${a.url}/catalog.json\ngucman repo add b ${b.url}/catalog.json`);
 const list=await s.command('gucman search hello');assert.match(list,/a:hello/);assert.match(list,/b:hello/);
 await s.command('if gucman build hello; then exit 99; fi');
 assert.match(await s.command('gucman build a:hello\n/usr/local/bin/hello'),/HELLO-A/);
 await s.command('if gucman build b:hello; then exit 99; fi');
 await s.command('if gucman build a:bad; then exit 99; fi\ntest ! -e /usr/local/bin/bad');
 await s.command('if gucman build a:cycle1; then exit 99; fi');
 await s.command('set -eu\nif gucman build a:collision; then exit 99; fi\ntest ! -e /usr/local/bin/dep');
 await s.command(`gucman repo add alias ${a.url}/catalog.json`);
 const aliased=fixture(a,'aliased','ALIASED',{dependencies:['a:dep','alias:dep']});catalog.packages.aliased=aliased.ref;a.put('/catalog.json',catalog);
 assert.match(await s.command('gucman build a:aliased\n/usr/local/bin/aliased'),/ALIASED/);
 await s.command('gucman repo remove alias');
 assert.match(await s.command('gucman build a:consumer\n/usr/local/bin/consumer\n/usr/local/bin/dep'),/CONSUMER/);
 // Updates must preserve the previous installed release when the recipe fails.
 const failedUpdate=fixture(a,'hello','CHANGED',{version:'2'});
 failedUpdate.m.files=failedUpdate.m.files.map(f=>f.path==='build.sh'?{path:f.path,...a.put('/fail-build.sh','exit 37\n')}:f);
 catalog.packages.hello=a.put('/failed-update.json',failedUpdate.m);a.put('/catalog.json',catalog);
 await s.command('if gucman build a:hello; then exit 99; fi');
 assert.match(await s.command('hello'),/HELLO-A/);
 catalog.packages.hello=alpha.ref;a.put('/catalog.json',catalog);
 // The local source label is not its identity.
 await s.command(`gucman repo remove a\ngucman repo add a ${b.url}/catalog.json`);
 await s.command('if gucman build a:hello; then exit 99; fi');
 assert.match(await s.command('hello'),/HELLO-A/);
 await s.command(`gucman repo remove a\ngucman repo add a ${a.url}/catalog.json`);
 await s.command('gucman repo remove b');
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('gucman repo list\n/usr/local/bin/hello'),/HELLO-A/);
 // Publish an actual binary built by the guest; install it on a fresh disk.
 const bytes=Buffer.from(await s.page.evaluate(async()=>Array.from(await shell.busybox.read('/usr/local/bin/hello'))));
 const binary=a.put('/hello.tar',sourceTar([['bin/hello',bytes,493],['share/licenses/hello/LICENSE',Buffer.from('Test fixture; public domain.\n')]]));
 const binaryManifest={...alpha.m,binary};catalog.packages.hello=a.put('/hello/binary.json',binaryManifest);a.put('/catalog.json',catalog);
 await s.close();s=await browserSession({allowedOrigins:[a.url]});
 await s.command(`set -eu\ngucman repo remove groundupcoder\ngucman repo add a ${a.url}/catalog.json\ngucman install hello\nhello`);
 // Platform/dependency mismatches fail before changing installed files.
 const platform={target:'wasm32-wasm-posix',minVersion:'1.0.0',runtimeAbi:5,objectAbi:3,features:['posix-core']};
 const installedBefore=await s.command('sha256sum /usr/local/lib/gucman/packages/hello/current/manifest.json');
 for(const change of [{platform:{...platform,minVersion:'2.0.0'}},{platform:{...platform,target:'wasm64'}},{platform:{...platform,features:['missing-feature']}},{platform,dependencies:['dep'],dependencyRequirements:{dep:{version:'999'}}}]){
  catalog.packages.hello=a.put('/incompatible.json',{...binaryManifest,...change});a.put('/catalog.json',catalog);
  await assert.rejects(()=>s.command('gucman upgrade hello'),/requirements not met/);
  assert.equal(await s.command('sha256sum /usr/local/lib/gucman/packages/hello/current/manifest.json'),installedBefore);
  assert.match(await s.command('hello'),/HELLO-A/);
 }
 catalog.packages.hello=a.put('/hello/binary.json',binaryManifest);a.put('/catalog.json',catalog);
 console.log('PASS incompatible platform/dependency graphs preserve installed manifest and executable');
 // Interrupt the real publication path before link creation, then recover it.
 const interrupted={...binaryManifest,version:'3'};
 catalog.packages.hello=a.put('/interrupted.json',interrupted);a.put('/catalog.json',catalog);
 await s.command(`set -eu
mv /usr/libexec/guc-links-v2 /tmp/original-links
cat > /usr/libexec/guc-links-v2 <<'FAULT'
#!/bin/sh
if [ "$1" = link ]; then kill -KILL "$(cat /usr/local/lib/gucman/transaction-owner)"; exit 1; fi
exec /tmp/original-links "$@"
FAULT
chmod 755 /usr/libexec/guc-links-v2`);
 try{await s.command('gucman upgrade hello');}catch(error){console.log('Interrupted command:',error.message);}
 await s.command('set -eu\ntest -f /usr/local/lib/gucman/pending/ready\ntest "$(/usr/libexec/guc-json string /usr/local/lib/gucman/packages/hello/current/manifest.json version)" = 1\nif kill -0 "$(cat /usr/local/lib/gucman/transaction-owner)" 2>/dev/null; then exit 99; fi');
 await s.command('set -eu\nmv /tmp/original-links /usr/libexec/guc-links-v2\ngucman recover\ntest ! -e /usr/local/lib/gucman/pending\ntest "$(/usr/libexec/guc-json string /usr/local/lib/gucman/packages/hello/current/manifest.json version)" = 3');
 assert.match(await s.command('hello'),/HELLO-A/);
 // Package outage cannot prevent core reload or installed command execution.
 a.files.delete('/catalog.json');
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);assert.match(await s.command('hello'),/HELLO-A/);
 await s.command('if gucman search hello; then exit 99; fi');
 await mkdir('test-results',{recursive:true});await writeFile('test-results/runtime-packages.json',JSON.stringify({passed:true,runtime:build.runtime,checks:['no package requests at boot','cross-origin catalogs','combined search','ambiguity refusal','explicit source build','source origin refusal','bad source hash refusal','dependency cycle refusal','ancestor conflict preflight','same-URL source aliases','dependency build','source config persistence','fresh binary install','binary reload','failed recipe preserves release','source label reuse refused','interrupted publication recovery','package-outage reload']},null,2)+'\n');
 console.log('PASS runtime repositories, guest builds and binary installs');
}finally{if(s)await s.close();await a.close();await b.close();}
