import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try{
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-symbols.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/ipc-symbols.c',import.meta.url),'utf8'));
 const log=await s.command('set -eu\ncc /tmp/ipc-symbols.c -o /tmp/ipc-symbols\n/tmp/ipc-symbols');console.log(log);assert.match(log,/IPC-SYMBOLS-PASS/);
}finally{await s.close();}
