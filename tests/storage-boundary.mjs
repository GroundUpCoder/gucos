import assert from 'node:assert/strict';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession();
const disks=['wasm-posix-os2-v1','wasm-posix-os2-v1.journal','wasm-posix-minimal-v1','wasm-posix-minimal-v1.journal','wasm-posix-busybox-v1','wasm-posix-busybox-v1.journal'];
const edits=['wasm-posix.os2.files','wasm-posix.minimal.files','wasm-posix.files'];
try{
 await s.page.evaluate(async({disks,edits})=>{
  const root=await navigator.storage.getDirectory();
  for(const name of disks){const f=await root.getFileHandle(name,{create:true}),w=await f.createWritable();await w.write('preserve:'+name);await w.close();}
  for(const name of edits)localStorage.setItem(name,'preserve:'+name);
 },{disks,edits});
 await s.command('echo disposable > /home/user/new-core-marker');
 s.page.on('dialog',dialog=>dialog.accept());
 await s.page.goto(s.url+'/?reset=1&shell=bootstrap');await s.page.waitForFunction(()=>window.shell?.ready);
 const contents=await s.page.evaluate(async({disks,edits})=>{
  const root=await navigator.storage.getDirectory(),out={};
  for(const name of disks)out[name]=await (await (await root.getFileHandle(name)).getFile()).text();
  for(const name of edits)out[name]=localStorage.getItem(name);
  return out;
 },{disks,edits});
 for(const name of [...disks,...edits])assert.equal(contents[name],'preserve:'+name);
 await s.command('test ! -e /home/user/new-core-marker');
 console.log('PASS new-core reset preserves all historical disk, journal and editor namespaces');
}finally{await s.close();}
