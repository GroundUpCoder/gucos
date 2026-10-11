import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({gpu:true});
try{
 await s.command('true');
 await s.page.evaluate(async source=>shell.busybox.write('/tmp/ime.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/platform-ime.c',import.meta.url),'utf8'));
 console.log(await s.command('set -eu\ncc /tmp/ime.c -o /tmp/ime\n/tmp/ime &'));
 await s.page.waitForFunction(()=>shell.busybox.log.includes('IME-READY'));
 const canvas=s.page.locator('.vt:visible canvas');await canvas.focus();
 await s.page.keyboard.down('Control');await s.page.keyboard.down('Shift');
 await canvas.evaluate(c=>{c.dispatchEvent(new CompositionEvent('compositionstart',{data:'',bubbles:true}));c.dispatchEvent(new KeyboardEvent('keyup',{key:'Shift',code:'ShiftLeft',ctrlKey:true,isComposing:true,bubbles:true}));});
 await s.page.waitForFunction(()=>shell.busybox.log.includes('UP:225:192:192'));
 await s.page.keyboard.up('Shift');await s.page.keyboard.up('Control');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('UP:224:0:0'));
 await canvas.evaluate(c=>{for(const [type,data] of [['compositionstart',''],['compositionupdate','ㅎ'],['compositionupdate','한'],['compositionend','한']])c.dispatchEvent(new CompositionEvent(type,{data,bubbles:true}));});
 await s.page.waitForFunction(()=>shell.busybox.log.includes('TEXT:한'));
 let log=await s.page.evaluate(()=>shell.busybox.log);assert.match(log,/EDIT:한:0:1/);assert.equal(log.split('TEXT:한').length-1,1);
 await canvas.press('q');await s.page.waitForFunction(()=>shell.busybox.log.includes('IME-STOPPED'));
 const before=await s.page.evaluate(()=>shell.busybox.log.length);
 await canvas.evaluate(c=>c.dispatchEvent(new CompositionEvent('compositionend',{data:'日',bubbles:true})));
 await canvas.press('x');await s.page.waitForFunction(()=>![...shell.busybox.windows.values()].some(w=>w.shown));
 assert.doesNotMatch((await s.page.evaluate(()=>shell.busybox.log)).slice(before),/TEXT:日/);
 console.log('PASS SDL composition updates, UTF-8 commit exactly once, stop suppression, unsupported window flags/fullscreen');
}finally{await s.close();}
