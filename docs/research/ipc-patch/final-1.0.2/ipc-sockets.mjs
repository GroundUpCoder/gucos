import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {browserSession} from './browser.mjs';
const source=await readFile(new URL('file:///tmp/gucos-ipc-final-acceptance-0vjl204x/fixtures/ipc-sockets.c'),'utf8');
const result={test:'ipc-sockets',fixtureSHA256:createHash('sha256').update(source).digest('hex'),status:'failed'};
let s;
try{
 s=await browserSession({timeout:180000});
 const identity=await s.page.evaluate(()=>fetch('/build.json').then(r=>r.json()));
 result.runtime=identity.runtime;console.log('IPC-SOCKET-RUNTIME '+identity.runtime);
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-sockets.c',new TextEncoder().encode(source)),source);
 result.log=await s.command('set -eu\ncc --enable-fork /tmp/ipc-sockets.c -o /tmp/ipc-sockets\n/tmp/ipc-sockets');
 console.log(result.log);assert.match(result.log,/IPC-SOCKET-PTY-PASS/);assert.match(result.log,/IPC-SOCKET-PASS/);
 result.status='passed';
}catch(error){result.error=String(error.stack||error);throw error;
}finally{
 if(s)await s.close();
 const directory=new URL('file:///tmp/gucos-ipc-final-acceptance-0vjl204x/results/');await mkdir(directory,{recursive:true});
 result.completedUTC=new Date().toISOString();
 await writeFile(new URL('ipc-sockets.json',directory),JSON.stringify(result,null,2)+'\n');
}
