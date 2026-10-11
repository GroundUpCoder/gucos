import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../../../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try{
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/ipc-adapted.c',new TextEncoder().encode(source)),await readFile(new URL('adapted-probes.c',import.meta.url),'utf8'));
 const log=await s.command('set -eu\ncc /tmp/ipc-adapted.c -o /tmp/ipc-adapted\n/tmp/ipc-adapted');
 console.log(log);assert.match(log,/IPC-UPSTREAM-ADAPTATIONS-PASS/);
}finally{await s.close();}
