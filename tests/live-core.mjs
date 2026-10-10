// Fresh-profile public release gate invoked while hosting operator.lock is held.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const expected=process.argv[2];assert.match(expected||'',/^[a-f0-9]{64}$/);
const options={...(process.env.LIVE_CORE_URL==='local'?{}:{url:process.env.LIVE_CORE_URL||'https://os.groundupcoder.net'}),allowedOrigins:['https://raw.githubusercontent.com'],gpu:true};
let s=await browserSession(options);
try {
 assert.equal(await s.page.evaluate(()=>crossOriginIsolated),true);
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),expected);
 console.log(await s.command('set -eu\ngucman install groundupcoder:jsmn\n/usr/local/bin/jsmn-example\necho LIVE-BINARY-PASS'));
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('/usr/local/bin/jsmn-example'),/UID: 1000/);
 await s.page.evaluate(async source=>shell.busybox.write('/tmp/gpu.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/webgpu-vt.c',import.meta.url),'utf8'));
 console.log(await s.command('set -eu\ncc /tmp/gpu.c -o /tmp/gpu\n/tmp/gpu live-one &\n/tmp/gpu live-two &'));
 await s.page.waitForFunction(()=>['live-one','live-two'].every(n=>shell.busybox.log.includes('GPU-FRAME-'+n)));
 await s.page.getByRole('tab',{name:/live-one/}).click();await s.page.locator('.vt:visible canvas').press('q');
 await s.page.getByRole('tab',{name:/live-two/}).click();await s.page.locator('.vt:visible canvas').press('q');
 await s.page.locator('#terminal-focus').click();
 assert.match(await s.command('echo LIVE-TERMINAL-PASS'),/LIVE-TERMINAL-PASS/);
}finally{await s.close();}
s=await browserSession(options);
try {
 assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),expected);
 assert.match(await s.command('set -eu\ngucman build groundupcoder:jsmn\n/usr/local/bin/jsmn-example'),/User: johndoe/);
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('/usr/local/bin/jsmn-example'),/UID: 1000/);
 console.log('PASS LIVE core '+expected+': fresh boot/compiler, terminal/two GPU VTs, public binary compatibility check and source build, persistence/reload');
}finally{await s.close();}
