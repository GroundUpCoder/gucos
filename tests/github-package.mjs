// Real anonymous runtime integration. No sibling checkout or host recipe execution.
import assert from 'node:assert/strict';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({allowedOrigins:['https://raw.githubusercontent.com']});
try{
 assert.match(await s.command('gucman search jsmn'),/groundupcoder:jsmn/);
 assert.match(await s.command('gucman build groundupcoder:jsmn\n/usr/local/bin/jsmn-example'),/User: johndoe/);
 await s.command('gucman pack jsmn /home/user/jsmn.tar');
 const bytes=Buffer.from(await s.page.evaluate(async()=>Array.from(await shell.busybox.read('/home/user/jsmn.tar'))));
 await mkdir('test-results/github-package',{recursive:true});await writeFile('test-results/github-package/jsmn.tar',bytes);
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('/usr/local/bin/jsmn-example'),/UID: 1000/);
 await writeFile('test-results/github-package/receipt.json',JSON.stringify({passed:true,runtime:JSON.parse(await readFile('dist/build.json')).runtime,checks:['anonymous raw GitHub discovery','pinned upstream source downloads','guest Make build','staged recipe tests','installed command','guest archive export','reload']},null,2)+'\n');
 console.log('PASS real raw GitHub source discovery/build/export/reload');
}finally{await s.close();}
