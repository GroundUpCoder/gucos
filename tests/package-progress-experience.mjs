import {chromium} from 'playwright';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {serve} from '../tools/serve.mjs';
const live=process.env.PROGRESS_URL;
const expected=process.env.EXPECTED_RUNTIME||JSON.parse(await readFile(new URL('../dist/build.json',import.meta.url))).runtime;
const dir=new URL('../docs/research/package-progress-1.0.1/'+(live?'live/':'candidate/'),import.meta.url);
await mkdir(dir,{recursive:true});
const site=live?{url:live,close:async()=>{}}:await serve();
const browser=await chromium.launch({channel:'chromium',headless:true});
const result={startedAt:new Date().toISOString(),url:site.url,runtime:expected,browser:browser.version(),errors:[]};
let context,timer;
try{
 context=await browser.newContext({viewport:{width:1000,height:700},recordVideo:{dir:new URL('video/',dir).pathname,size:{width:1000,height:700}}});
 const page=await context.newPage();page.setDefaultTimeout(240000);
 page.on('pageerror',e=>result.errors.push(e.message));
 await page.goto(site.url);await page.waitForFunction(()=>window.shell?.busybox?.ready&&shell.busybox.log.includes('~ $ '));
 assert.equal(await page.evaluate(()=>shell.toolchain.systemBuild),expected);
 await page.evaluate(()=>{
  window.measure={enter:0,first:null,done:null,events:[]};
  document.addEventListener('keydown',e=>{if(e.key==='Enter'&&!measure.enter){measure.enter=performance.now();measure.start=shell.busybox.log.length;}},true);
  shell.terminal.onRender(()=>{
   if(!measure.enter)return;
   const log=shell.busybox.log.slice(measure.start);
   const lines=[...log.matchAll(/^(gucman: .+|  Downloaded .+)$/gm)];
   for(let i=measure.events.length;i<lines.length;i++)measure.events.push({ms:performance.now()-measure.enter,text:lines[i][0]});
   const b=shell.terminal.buffer.active;let screen='';for(let y=b.viewportY;y<b.viewportY+shell.terminal.rows;y++)screen+=(b.getLine(y)?.translateToString(true)||'')+'\n';
   const paint=fn=>requestAnimationFrame(()=>requestAnimationFrame(fn));
   if(!measure.first&&screen.includes('gucman: install jwm:'))paint(()=>{if(!measure.first)measure.first=performance.now()-measure.enter;});
   if(!measure.done&&log.includes('Installed groundupcoder:jwm')&&/~ \$\s*$/.test(screen))paint(()=>{if(!measure.done)measure.done=performance.now()-measure.enter;});
  });
 });
 await page.locator('.xterm-helper-textarea').fill('gucman install jwm');await page.locator('.xterm-helper-textarea').press('Enter');
 await page.waitForFunction(()=>measure.first!==null,null,{timeout:10000});
 console.log('First visible progress (ms):',await page.evaluate(()=>measure.first));
 timer=setInterval(async()=>{try{console.log(await page.evaluate(()=>measure.events.slice(-2)));}catch{}},30000);
 await page.waitForFunction(()=>measure.done!==null,null,{timeout:600000});clearInterval(timer);
 result.measurement=await page.evaluate(()=>({...measure,log:shell.busybox.log.slice(measure.start)}));
 assert.match(result.measurement.log,/Installing x11-server: [1-9][0-9]*\/[0-9]+ files/);
 assert.match(result.measurement.log,/Package 4\/4: jwm/);
 assert.deepEqual(result.errors,[]);
 await page.screenshot({path:new URL('completed.png',dir).pathname});
 await page.waitForTimeout(1500);
 const video=page.video();await context.close();context=null;result.video=await video.path();result.status='passed';
 console.log(JSON.stringify({status:result.status,firstMs:result.measurement.first,totalMs:result.measurement.done}));
} catch(e){result.status='failed';result.failure=e.stack;throw e;}finally{
 clearInterval(timer);if(context)await context.close();await browser.close();await site.close();
 await writeFile(new URL('result.json',dir),JSON.stringify(result,null,2));
}
