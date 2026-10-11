import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {appBrowser} from '../../../tests/app-browser.mjs';
import {runCached} from '../../../tests/package-cache.mjs';
const {page,errors,close}=await appBrowser('busybox',1700000);
try {
 const source=await readFile(new URL('./busybox-accept.sh',import.meta.url),'utf8');
 const command=process.env.WASM_POSIX_PACKAGE_PHASE==='binary'?'gucman install --force busybox':'gucman rebuild busybox';
 const script=source.replace('gucman install busybox',command);
 const result=await runCached(page,script,{packages:['busybox'],fresh:['busybox']});
 const log=await page.evaluate(()=>shell.busybox.log);
 assert.equal(result.code,0,log);
 assert.ok(log.includes('BUSYBOX-ACCEPTANCE-PASS'),log);
 assert.deepEqual(errors,[]);
} finally {await close();}
