import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try{
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-sockets.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/ipc-sockets.c',import.meta.url),'utf8'));
 const log=await s.command('set -eu\ncc --enable-fork /tmp/ipc-sockets.c -o /tmp/ipc-sockets\n/tmp/ipc-sockets');
 console.log(log);assert.match(log,/IPC-SOCKET-PASS/);
}finally{await s.close();}

