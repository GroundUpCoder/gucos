import assert from 'node:assert/strict';
import {browserSession as original} from '/home/ubuntu/git/gucos-closefrom/tools/browser.mjs';
export async function browserSession(options={}){
 const s=await original({...options,directory:"/home/ubuntu/git/gucos-core-snapshots/ipc-closefrom-e1a1d35a"});
 try{
 const identity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));
 assert.equal(identity.runtime,"e1a1d35aab446d25eb19126cec01e17e5508b672bc16f73b4cbb5e0a01f3b8a8");
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),identity.runtime);
 console.log('VERIFIED-FROZEN-RUNTIME '+identity.runtime);return s;
 }catch(e){await s.close();throw e;}
}
