import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {browserSession} from '../../../tools/browser.mjs';
const source=await readFile(new URL('adapted-probes.c',import.meta.url),'utf8');
const result={test:'ipc-adapted',fixtureSHA256:createHash('sha256').update(source).digest('hex'),status:'failed'};
let s;
try{
 s=await browserSession({timeout:180000});
 const identity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));
 result.runtime=identity.runtime;console.log('IPC-ADAPTED-RUNTIME '+identity.runtime);
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-adapted.c',new TextEncoder().encode(source)),source);
 result.log=await s.command('set -eu\ncc /tmp/ipc-adapted.c -o /tmp/ipc-adapted\n/tmp/ipc-adapted');
 console.log(result.log);assert.match(result.log,/IPC-UPSTREAM-ADAPTATIONS-PASS/);
 result.status='passed';
}catch(error){result.error=String(error.stack||error);throw error;
}finally{
 if(s)await s.close();
 const directory=new URL('../../../test-results/',import.meta.url);await mkdir(directory,{recursive:true});
 result.completedUTC=new Date().toISOString();
 await writeFile(new URL('ipc-adapted.json',directory),JSON.stringify(result,null,2)+'\n');
}
