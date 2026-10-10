import {chromium} from 'playwright';
import {mkdir,writeFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
const out=new URL('./',import.meta.url);
const expected='c4411d720bede49ead5c1de69bfe7b1247656a146b72d991d209883606e300fb';
const report={startedAt:new Date().toISOString(),url:'https://os.groundupcoder.net/',expectedRuntime:expected,boot:[],commands:[],errors:[]};
await mkdir(new URL('video/',out),{recursive:true});
const browser=await chromium.launch({channel:'chromium',headless:true});
report.browser=browser.version();
async function save(){await writeFile(new URL('results.json',out),JSON.stringify(report,null,2));}
function instrumentation(){
 window.probe={boot:null,active:null};
 function screen(){const b=shell.terminal.buffer.active;let lines=[];for(let y=b.viewportY;y<b.viewportY+shell.terminal.rows;y++)lines.push(b.getLine(y)?.translateToString(true)||'');return lines.join('\n').trimEnd();}
 function painted(fn){requestAnimationFrame(()=>requestAnimationFrame(fn));}
 const timer=setInterval(()=>{
  if(!window.shell?.terminal)return;clearInterval(timer);
  shell.terminal.onRender(()=>{
   const text=screen();
   if(!probe.boot&&window.shell?.busybox?.ready&&/\~ \$\s*$/.test(text))painted(()=>{if(!probe.boot)probe.boot={ms:performance.now(),text};});
   const a=probe.active;if(!a?.enter)return;
   if(!a.firstResponse&&/(: OK|Installed |Already installed:|gucman:|Building )/.test(shell.busybox.log.slice(a.logStart))&&/(: OK|Installed |Already installed:|gucman:|Building )/.test(text)){
    const line=text.split('\n').find(x=>/(: OK|Installed |Already installed:|gucman:|Building )/.test(x));
    painted(()=>{if(!a.firstResponse)a.firstResponse={ms:performance.now()-a.enter,text:line};});
   }
   if(!a.done&&/\~ \$\s*$/.test(text)&&shell.busybox.log.slice(a.logStart).includes('\n'))painted(()=>{if(!a.done)a.done={ms:performance.now()-a.enter,text};});
  });
 },5);
 document.addEventListener('keydown',e=>{if(e.key==='Enter'&&probe.active&&!probe.active.enter){probe.active.enter=performance.now();probe.active.logStart=shell.busybox.log.length;}},true);
}
try{
 for(let pair=0;pair<3;pair++){
  const context=await browser.newContext({viewport:{width:1000,height:700},...(pair===0?{recordVideo:{dir:new URL('video/',out).pathname,size:{width:1000,height:700}}}:{})});
  await context.addInitScript(instrumentation);
  const page=await context.newPage();page.setDefaultTimeout(300000);
  page.on('pageerror',e=>report.errors.push({pair,error:e.message}));
  for(const mode of ['fresh','reload']){
   if(mode==='fresh')await page.goto(report.url,{waitUntil:'domcontentloaded'});else await page.reload({waitUntil:'domcontentloaded'});
   await page.waitForFunction(()=>probe.boot);
   const result=await page.evaluate(()=>({boot:probe.boot,runtime:shell.toolchain.systemBuild,resources:performance.getEntriesByType('resource').map(r=>({name:r.name,start:r.startTime,duration:r.duration,transferSize:r.transferSize}))}));
   assert.equal(result.runtime,expected);
   report.boot.push({pair,mode,recorded:pair===0,...result});await save();console.log(JSON.stringify({pair,mode,ms:result.boot.ms}));
   await page.waitForTimeout(1200);
  }
  if(pair===0){
   for(const name of ['jsmn','jwm','xterm']){
    // Clear the viewport before each command so prior progress cannot satisfy the probe.
    await page.locator('.xterm-helper-textarea').fill("printf '\\033[2J\\033[H'");await page.locator('.xterm-helper-textarea').press('Enter');await page.waitForTimeout(500);
    const command='gucman install '+name;
    await page.locator('.xterm-helper-textarea').fill(command);await page.waitForTimeout(500);
    await page.evaluate(command=>{probe.active={command,enter:null,firstResponse:null,done:null};},command);
    await page.locator('.xterm-helper-textarea').press('Enter');
    const heartbeat=setInterval(async()=>{try{console.log(JSON.stringify(await page.evaluate(()=>({command:probe.active.command,elapsedMs:performance.now()-probe.active.enter,firstResponse:probe.active.firstResponse,tail:shell.busybox.log.slice(-180)}))));}catch{}},30000);
    try{await page.waitForFunction(()=>probe.active.done,null,{timeout:600000});}finally{clearInterval(heartbeat);}
    const measurement=await page.evaluate(()=>({...probe.active,log:shell.busybox.log.slice(probe.active.logStart)}));
    assert.match(measurement.log,new RegExp('Installed [^\\n]*:'+name+' '));
    report.commands.push(measurement);console.log(JSON.stringify({command,firstResponse:measurement.firstResponse,totalMs:measurement.done.ms}));await save();
    await page.evaluate(()=>probe.active=null);await page.waitForTimeout(1500);
   }
   await page.screenshot({path:new URL('installed.png',out).pathname});
   await page.locator('.xterm-helper-textarea').fill('start-jwm');await page.locator('.xterm-helper-textarea').press('Enter');
   await page.waitForSelector('.vt:visible canvas');await page.waitForTimeout(4000);
   await page.screenshot({path:new URL('desktop.png',out).pathname});
   report.desktop={visibleCanvas:true};await save();
  }
  const video=page.video();await context.close();if(video)report.video=await video.path();await save();
 }
 report.finishedAt=new Date().toISOString();report.status='passed';await save();
} catch(e){report.status='failed';report.failure=e.stack;await save();throw e;}finally{await browser.close();}
