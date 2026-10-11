import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from './browser.mjs';
const s=await browserSession();
try{
 await s.command("true");
 await s.page.evaluate(async source=>shell.busybox.write('/tmp/platform.c',new TextEncoder().encode(source)),await readFile(new URL('file:///tmp/gucos-closefrom-regressions-iio3kww1/fixtures/platform-posix.c'),'utf8'));
 const log=await s.command('set -eu\ncc /tmp/platform.c -o /tmp/platform\n/tmp/platform');console.log(log);assert.match(log,/PLATFORM-POSIX-PASS/);
}finally{await s.close();}
