import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {browserSession} from '/home/ubuntu/git/gucos-closefrom/tools/browser.mjs';
const out=new URL('.',import.meta.url),result={status:'failed',checks:[]};
const expected='e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8';
const s=await browserSession({directory:'/home/ubuntu/git/gucos-core-snapshots/ipc-closefrom-e1a1d35a',timeout:180000});
async function cmd(name,script){
 try{const log=await s.command(script);console.log(name+'\n'+log);await writeFile(new URL(name+'.log',out),log);return log;}
 catch(e){await writeFile(new URL(name+'.log',out),String(e.stack||e));throw e;}
}
try{
 result.identity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));assert.equal(result.identity.runtime,expected);
 await cmd('prepare','mkdir -p /tmp/contract');
 for(const name of ['provider','plain','fork'])await s.page.evaluate(async({name,text})=>shell.busybox.write('/tmp/contract/'+name+'.c',new TextEncoder().encode(text)),{name,text:await readFile(new URL(name+'.c',out),'utf8')});
 await cmd('compile',[
 'set -eu','cd /tmp/contract','cc -c provider.c -o provider.o',
 'ar rcs libprovider.a provider.o',
 'cc -shared provider.o -Wl,-soname,libplain-provider.so -o libplain-provider.so',
 'cc --enable-fork -shared provider.o -Wl,-soname,libfork-provider.so -o libfork-provider.so',
 'cc plain.c /tmp/contract/libplain-provider.so -Wl,-rpath,/tmp/contract -o plain-app',
 'cc --enable-fork fork.c /tmp/contract/libprovider.a -o archive-app',
 'cc --enable-fork fork.c /tmp/contract/libfork-provider.so -Wl,-rpath,/tmp/contract -o fork-dso-app',
 'cc --enable-fork fork.c /tmp/contract/libplain-provider.so -Wl,-rpath,/tmp/contract -o mixed-app'].join('\n'));
 for(const [name,marker]of [['plain-app',/PLAIN-42/],['archive-app',/FORK-42/],['fork-dso-app',/FORK-42/]]){
 assert.match(await cmd(name,'/tmp/contract/'+name),marker);result.checks.push({name,status:'passed'});
 }
 try{await cmd('mixed-app','/tmp/contract/mixed-app');throw Error('Unexpected mixed DSO success');}
 catch(e){assert.match(String(e),/execution contract mismatch: fprintf/);result.checks.push({name:'mixed-app',status:'expected rejection',error:String(e)});}
 result.artifacts={};
 for(const name of ['provider.o','libprovider.a','libplain-provider.so','libfork-provider.so','plain-app','archive-app','fork-dso-app','mixed-app']){
 const bytes=Buffer.from(await s.page.evaluate(async path=>Array.from(await shell.busybox.read(path)),'/tmp/contract/'+name));
 await writeFile(new URL(name,out),bytes);result.artifacts[name]={size:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')};
 if(bytes[0]===0 && name.endsWith('.so')){
 const module=new WebAssembly.Module(bytes);result.artifacts[name].dylink=JSON.parse(new TextDecoder().decode(WebAssembly.Module.customSections(module,'wasmposix.dylink')[0]));
 }
 }
 result.status='passed';console.log('FORK-LINK-CONTRACT-PASS');
}catch(e){result.error=String(e.stack||e);console.error(result.error);process.exitCode=1;}
finally{await s.close();await writeFile(new URL('results.json',out),JSON.stringify(result,null,2)+'\n');}
