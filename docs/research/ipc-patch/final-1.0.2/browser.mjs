import assert from 'node:assert/strict';
import {browserSession as original} from '/home/ubuntu/git/gucos-ipc-final/tools/browser.mjs';
export async function browserSession(options={}){
 const s=await original({...options,directory:"/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7"});
 try{
 const identity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));
 assert.equal(identity.runtime,"be3a9ce7817c876e8d9bc900556fea1fbaeb4f0d5d4e03003ab24a7ad3b600bb");
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),identity.runtime);
 console.log('VERIFIED-FINAL-RUNTIME '+identity.runtime);return s;
 }catch(e){await s.close();throw e;}
}
