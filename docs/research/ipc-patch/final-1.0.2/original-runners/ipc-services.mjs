import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({timeout:180000});
try {
  await s.command('true');
  await s.page.evaluate(async source=>shell.busybox.write('/tmp/ipc-services.c',new TextEncoder().encode(source)),
    await readFile(new URL('fixtures/ipc-services.c',import.meta.url),'utf8'));
  const log=await s.command('set -eu\ncc --enable-fork /tmp/ipc-services.c -o /tmp/ipc-services\n/tmp/ipc-services');
  console.log(log);assert.match(log,/IPC-SERVICES-PASS/);
} finally { await s.close(); }
