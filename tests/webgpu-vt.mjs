import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({gpu:true});
try {
 await s.command('true');
 await s.page.evaluate(async source=>shell.busybox.write('/tmp/gpu.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/webgpu-vt.c',import.meta.url),'utf8'));
 console.log(await s.command('cc /tmp/gpu.c -o /tmp/gpu\n/tmp/gpu gpu-one &\n/tmp/gpu gpu-two &'));
 await s.page.waitForFunction(()=>['gpu-one','gpu-two'].every(n=>shell.busybox.log.includes('GPU-FRAME-'+n)));
 await s.page.getByRole('tab',{name:/gpu-one/}).click();
 await s.page.setViewportSize({width:750,height:530});
 await s.page.waitForTimeout(500);
 await s.page.locator('.vt:visible canvas').press('x');
 await s.page.waitForFunction(()=>![...shell.busybox.windows.values()].some(v=>v.title.textContent.includes('gpu-one')));
 await s.page.getByRole('tab',{name:/gpu-two/}).click();
 const n=await s.page.evaluate(()=>shell.busybox.log.length);await s.page.waitForTimeout(1200);
 assert.match(await s.page.evaluate(n=>shell.busybox.log.slice(n),n),/GPU-FRAME-gpu-two/);
 await s.page.locator('.vt:visible canvas').press('q');
 await s.page.waitForFunction(()=>[...shell.busybox.windows.values()].every(v=>!v.shown));
 assert.match(await s.command('echo GPU-RECOVERY-PASS'),/GPU-RECOVERY-PASS/);
 console.log('PASS: two direct WebGPU applications through SDL bridge, resize, independent crash/continued device progress and terminal recovery');
} finally {await s.close();}
