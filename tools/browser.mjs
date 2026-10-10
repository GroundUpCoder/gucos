import {chromium} from 'playwright';
import {serve} from './serve.mjs';
export const quote=s=>"'"+s.replaceAll("'","'\\''")+"'";
export async function browserSession(options={}){
 const site=options.url?{url:new URL(options.url).origin,close:async()=>{}}:await serve(options);let browser;
 try{
  browser=await chromium.launch({channel:'chromium',headless:true,args:options.gpu?['--enable-unsafe-webgpu']:[]});
  const context=await browser.newContext();
  const errors=[];
  await context.route('**/*',route=>[site.url,...(options.allowedOrigins||[])].includes(new URL(route.request().url()).origin)?route.continue():route.abort());
  const page=await context.newPage();page.setDefaultTimeout(180000);page.on('pageerror',e=>errors.push(e.message));
  const timer=setTimeout(()=>void browser.close(),options.timeout||600000);
  const close=async()=>{clearTimeout(timer);await browser.close();await site.close();};
  try{await page.goto(site.url+'/?shell=bootstrap');await page.waitForFunction(()=>window.shell?.ready);}catch(e){await close();throw e;}
  async function command(script){
   const before=await page.evaluate(()=>(shell.busybox?.log||'').length);
   const result=await page.evaluate(s=>shell.command(s),'sh -c '+quote(script));
   const log=await page.evaluate(n=>(shell.busybox?.log||'').slice(n),before);
   if(result.code!==0)throw Error('Guest exit '+result.code+'\n'+log);
   if(errors.length)throw Error(errors.join('\n'));
   return log;
  }
  return {page,command,url:site.url,close,errors};
 }catch(e){if(browser)await browser.close();await site.close();throw e;}
}
