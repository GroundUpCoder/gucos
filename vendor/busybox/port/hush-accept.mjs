import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {appBrowser} from '../../../tests/app-browser.mjs';
import {runCached} from '../../../tests/package-cache.mjs';
const {page,errors,close}=await appBrowser('hush',1700000);
try {
 const source=await readFile(new URL('./hush-accept.sh',import.meta.url),'utf8');
 const command=process.env.WASM_POSIX_PACKAGE_PHASE==='binary'?'gucman install --force hush':'gucman rebuild hush';
 const script=source.replace('gucman install hush',command);
 const result=await runCached(page,script,{packages:['hush'],fresh:['hush']});
 const log=await page.evaluate(()=>shell.busybox.log);
 assert.equal(result.code,0,log);
 assert.ok(log.includes('HUSH-ACCEPTANCE-PASS'),log);
 for(const [label,wrong] of [['result',source.replace(' = 42',' = 43')],['exit-status',source.replace(' = 7',' = 8')]]) {
  assert.notEqual(wrong,source,'negative control must change its assertion');
  // Use the installed candidate without rebuilding/reinstalling it.
  const negative=wrong.replace(/^gucman .*$/m,'');
  const observed=await page.evaluate(async ({negative,label})=>{
   const start=shell.busybox.log.length;
   const path='/tmp/hush-negative-'+label+'.sh';
   shell.ui.save(path,negative);
   const result=await shell.command('sh '+path);
   return {code:result.code,log:shell.busybox.log.slice(start)};
  },{negative,label});
  assert.notEqual(observed.code,0,label+' false assertion must fail: '+observed.log);
  assert.ok(!observed.log.includes('HUSH-ACCEPTANCE-PASS'),observed.log);
  console.log('PASS hush negative control '+label+': rejected with exit '+observed.code);
 }
 assert.deepEqual(errors,[]);
} finally {await close();}
