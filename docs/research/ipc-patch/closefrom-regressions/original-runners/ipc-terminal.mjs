import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try{
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-terminal.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/ipc-terminal.c',import.meta.url),'utf8'));
 const log=await s.command('set -eu\ncc --enable-fork /tmp/ipc-terminal.c -o /tmp/ipc-terminal\n/tmp/ipc-terminal');
 console.log(log);assert.match(log,/IPC-TERMINAL-PASS/);
}finally{await s.close();}
