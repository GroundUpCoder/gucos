import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try{
 await s.command('true');
 console.log('RUNTIME '+await s.page.evaluate(()=>shell.toolchain.systemBuild));
 for(const name of ['closefrom-symbol','closefrom']){
  await s.page.evaluate(async({name,source})=>shell.busybox.write('/tmp/'+name+'.c',new TextEncoder().encode(source)),{name,source:await readFile(new URL('fixtures/'+name+'.c',import.meta.url),'utf8')});
  const log=await s.command('set -eu\ncc '+(name==='closefrom'?'--enable-fork ':'')+'/tmp/'+name+'.c -o /tmp/'+name+'\n/tmp/'+name);
  console.log(log);assert.match(log,name==='closefrom'?/CLOSEFROM-PASS/:/CLOSEFROM-SYMBOL-PASS/);
 }
}finally{await s.close();}
