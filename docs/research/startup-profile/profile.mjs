import {chromium} from 'playwright';
import {writeFile} from 'node:fs/promises';
const dir=new URL('./',import.meta.url);
const browser=await chromium.launch({channel:'chromium',headless:true});
const cdp=await browser.newBrowserCDPSession();
const samples=[];const tracing=process.env.PROFILE_TRACE!=='0';
try {
 if(tracing)await cdp.send('Tracing.start',{categories:'devtools.timeline,v8,disabled-by-default-v8.cpu_profiler,loading',transferMode:'ReturnAsStream'});
 for(let pair=0;pair<3;pair++){
  const context=await browser.newContext();
  await context.addInitScript(()=>{
   window.bootProbe={events:[],longTasks:[]};
   const mark=(kind,extra={})=>bootProbe.events.push({ms:performance.now(),kind,...extra});
   new PerformanceObserver(list=>{for(const e of list.getEntries())bootProbe.longTasks.push({start:e.startTime,duration:e.duration});}).observe({type:'longtask',buffered:true});
   const Native=window.Worker;
   window.Worker=class extends Native {constructor(url,options){super(url,options);mark('worker-created',{url:String(url),name:options?.name});this.addEventListener('message',e=>{const d=e.data;if(['ready','output','built','error'].includes(d?.type))mark('worker-'+d.type,{text:d.text?.slice(0,300)});});}};
   new MutationObserver(()=>{const text=document.getElementById('status')?.textContent;if(text&&text!==bootProbe.status){bootProbe.status=text;mark('status',{text});}}).observe(document,{subtree:true,childList:true,characterData:true});
   const timer=setInterval(()=>{if(window.shell?.ready){mark('shell-ready');clearInterval(timer);}},5);
  });
  const page=await context.newPage();page.setDefaultTimeout(180000);
  for(const mode of ['fresh','reload']){
   const started=Date.now();
   if(mode==='fresh')await page.goto('https://os.groundupcoder.net/',{waitUntil:'domcontentloaded'});else await page.reload({waitUntil:'domcontentloaded'});
   await page.waitForFunction(()=>window.shell?.busybox?.ready);
   await page.waitForFunction(()=>window.shell.busybox.log.includes('$ '));
   await page.waitForTimeout(100);
   const result=await page.evaluate(()=>({probe:bootProbe,navigation:performance.getEntriesByType('navigation').map(x=>x.toJSON()),resources:performance.getEntriesByType('resource').map(x=>x.toJSON()),runtime:shell.toolchain.systemBuild,log:shell.busybox?.log}));
   samples.push({pair,mode,wallMs:Date.now()-started,...result});
   await writeFile(new URL(tracing?'results.json':'baseline-results.json',dir),JSON.stringify({recordedAt:new Date().toISOString(),browser:browser.version(),url:'https://os.groundupcoder.net/',samples},null,2));
   console.log(JSON.stringify({pair,mode,wallMs:Date.now()-started,events:result.probe.events}));
  }
  await context.close();
 }
 if(tracing){const done=new Promise(resolve=>cdp.once('Tracing.tracingComplete',resolve));await cdp.send('Tracing.end');const {stream}=await done;
 let chunks=[];while(true){const r=await cdp.send('IO.read',{handle:stream});chunks.push(r.base64Encoded?Buffer.from(r.data,'base64'):Buffer.from(r.data));if(r.eof)break;}
 await cdp.send('IO.close',{handle:stream});await writeFile(new URL('trace.json',dir),Buffer.concat(chunks));}
}finally{await browser.close();}
