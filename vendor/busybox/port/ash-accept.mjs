import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {appBrowser} from '../../../tests/app-browser.mjs';
import {runCached} from '../../../tests/package-cache.mjs';
const {page,errors,close}=await appBrowser('ash',1700000);
try {
 const source=await readFile(new URL('./ash-accept.sh',import.meta.url),'utf8');
 const command=process.env.WASM_POSIX_PACKAGE_PHASE==='binary'?'gucman install --force ash':'gucman rebuild ash';
 const script=source.replace('gucman install ash',command);
 const result=await runCached(page,script,{packages:['ash'],fresh:['ash']});
 const log=await page.evaluate(()=>shell.busybox.log);
 assert.equal(result.code,0,log);
 assert.ok(log.includes('ASH-ACCEPTANCE-PASS'),log);
 for(const [label,wrong] of [['result',source.replace(' = 42',' = 43')],['exit-status',source.replace(' = 7',' = 8')]]) {
  assert.notEqual(wrong,source,'negative control must change its assertion');
  // Use the installed candidate without rebuilding/reinstalling it.
  const negative=wrong.replace(/^gucman .*$/m,'');
  const observed=await page.evaluate(async ({negative,label})=>{
   const start=shell.busybox.log.length;
   const path='/tmp/ash-negative-'+label+'.sh';
   shell.ui.save(path,negative);
   const result=await shell.command('sh '+path);
   return {code:result.code,log:shell.busybox.log.slice(start)};
  },{negative,label});
  assert.notEqual(observed.code,0,label+' false assertion must fail: '+observed.log);
  assert.ok(!observed.log.includes('ASH-ACCEPTANCE-PASS'),observed.log);
  console.log('PASS ash negative control '+label+': rejected with exit '+observed.code);
 }
 assert.deepEqual(errors,[]);
} finally {await close();}
