import assert from 'node:assert/strict';
import {chromium} from 'playwright';
import {mkdir,readFile} from 'node:fs/promises';
import {serve} from '../tools/serve.mjs';
const live=process.env.STARTUP_TEST_URL;
const site=live?{url:live,close:async()=>{}}:await serve();
// Serve the first real binary in controlled chunks to test visible byte progress.
let nextChunk,finishChunks,release;const gate=new Promise(resolve=>release=resolve);
const secondChunk=new Promise(resolve=>nextChunk=resolve),lastChunks=new Promise(resolve=>finishChunks=resolve);
if(!live){
 const original=site.server.listeners('request')[0];let chunked=false;site.server.removeAllListeners('request');
 site.server.on('request',async(req,res)=>{
  if(chunked||!req.url.endsWith('.bin'))return original(req,res);
  chunked=true;
  const bytes=await readFile(new URL('../dist'+req.url,import.meta.url));
  if(bytes.length<65536){chunked=false;return original(req,res);}
  chunked=true;console.log('Controlled stream',req.url,bytes.length);await gate;
  res.writeHead(200,{'Content-Type':'application/octet-stream','Content-Length':bytes.length,'Cross-Origin-Resource-Policy':'same-origin'});
  res.write(bytes.subarray(0,32768));await secondChunk;await new Promise(r=>setTimeout(r,150));
  res.write(bytes.subarray(32768,65536));await lastChunks;res.end(bytes.subarray(65536));
 });
}
const browser=await chromium.launch({channel:'chromium',headless:true});
await mkdir('test-results/startup',{recursive:true});
try {
 const context=await browser.newContext({viewport:{width:375,height:780}});
 const page=await context.newPage();page.setDefaultTimeout(120000);
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 let held=false;
 if(live)await context.route('**/*.bin',async route=>{if(!held){held=true;await gate;}await route.continue();});
 await page.goto(site.url+'/');
 await page.waitForFunction(()=>document.querySelector('#startup-downloads li[data-state="downloading"]'));
 assert.equal(await page.locator('#startup').isVisible(),true);
 const freshCount=await page.locator('#startup-downloads li').count();assert.equal(freshCount,14);
 assert.match(await page.locator('#startup-summary').textContent(),/remaining/);
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
 await page.screenshot({path:'test-results/startup/downloading-375.png'});
 release();
 if(!live){
  await page.waitForFunction(()=>[...document.querySelectorAll('#startup-downloads li[data-state="downloading"]')].some(row=>row.textContent.includes('32 KB /')),null,{timeout:15000}).catch(async error=>{console.log(await page.locator('#startup').textContent());throw error;});
  nextChunk();
  await page.waitForFunction(()=>[...document.querySelectorAll('#startup-downloads li[data-state="downloading"]')].some(row=>row.textContent.includes('64 KB /')));
  finishChunks();
 }
 await page.waitForFunction(()=>window.shell?.busybox?.ready&&shell.busybox.log.includes('~ $ '));
 assert.match(await page.locator('#startup-summary').textContent(),/^14 of 14 downloads ready · 0 remaining$/);
 assert.equal(await page.locator('#startup-downloads li[data-state="done"]').count(),14);
 const expected=process.env.EXPECTED_RUNTIME;if(expected)assert.equal(await page.evaluate(()=>shell.toolchain.systemBuild),expected);
 await page.waitForFunction(()=>document.getElementById('startup').hidden);
 await page.locator('#startup-toggle').click();assert.equal(await page.locator('#startup').isVisible(),true);
 await page.locator('#startup-hide').click();
 assert.equal(await page.evaluate(()=>document.activeElement.id),'startup-toggle');
 await page.locator('.xterm-helper-textarea').fill('echo STARTUP-INPUT-PASS');await page.locator('.xterm-helper-textarea').press('Enter');
 await page.waitForFunction(()=>shell.busybox.log.includes('STARTUP-INPUT-PASS\r\n')||shell.busybox.log.includes('STARTUP-INPUT-PASS\n'));
 await page.reload();await page.waitForFunction(()=>window.shell?.busybox?.ready&&shell.busybox.log.includes('~ $ '));
 assert.equal(await page.locator('#startup-downloads li').count(),5);
 assert.match(await page.locator('#startup-summary').textContent(),/^5 of 5 downloads ready · 0 remaining$/);
 assert.equal(errors.length,0,errors.join('\n'));
 await context.close();
 console.log('PASS startup fresh, byte/download states, exact remaining counts, prompt/input, history, 375px and persisted reload');
 for(const failure of ['http','checksum']){
  const c=await browser.newContext();const p=await c.newPage();p.setDefaultTimeout(120000);let broken=false;
  await c.route('**/*.bin',async route=>{if(!broken){broken=true;await route.fulfill({status:failure==='http'?503:200,body:'invalid bootstrap bytes'});}else await route.continue();});
  await p.goto(site.url+'/');await p.waitForFunction(()=>document.getElementById('startup-title').textContent==='Startup could not finish');
  assert.equal(await p.locator('#startup').isVisible(),true);assert.equal(await p.locator('#startup-retry').isVisible(),true);
  assert.ok(await p.locator('#startup-downloads li[data-state="failed"]').count()>0);
  assert.match(await p.locator('#startup-summary').textContent(),failure==='http'?/503/:/verification failed/);
  await c.unroute('**/*.bin');await p.locator('#startup-retry').click();
  await p.waitForFunction(()=>window.shell?.busybox?.ready&&shell.busybox.log.includes('~ $ '));
  await c.close();console.log('PASS startup '+failure+' failure stays visible and retry succeeds');
 }
}finally{release();nextChunk();finishChunks();await browser.close();await site.close();}
