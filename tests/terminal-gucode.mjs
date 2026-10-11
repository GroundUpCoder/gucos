// Exercise the publicly installed application's editor on the candidate core.
// Local slash commands only: no model/provider requests or real credentials.
import assert from 'node:assert/strict';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({...(process.env.LIVE_CORE_URL?{url:process.env.LIVE_CORE_URL}:{}),allowedOrigins:['https://raw.githubusercontent.com'],timeout:600000});
try{
 if(process.env.EXPECTED_RUNTIME)assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),process.env.EXPECTED_RUNTIME);
 console.log(await s.command('set -eu\ngucman install groundupcoder:gucode\ngucode --version'));
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('gucode --version'),/0\.1\.2/);
 const start=await s.page.evaluate(()=>shell.busybox.log.length);
 await s.page.locator('.xterm-helper-textarea').focus();
 await s.page.keyboard.type('sh');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(n=>shell.busybox.log.slice(n).includes('$ '),start);
 await s.page.keyboard.type('DEEPSEEK_API_KEY=synthetic-test-key gucode');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('gucode> '));
 await s.page.keyboard.type('exit');await s.page.keyboard.press('Control+a');
 await s.page.keyboard.type('/');await s.page.keyboard.press('Control+e');
 await s.page.waitForFunction(()=>{const b=shell.terminal.buffer.active;return Array.from({length:b.length},(_,i)=>b.getLine(i)?.translateToString(true)).some(l=>l==='gucode> /exit');});
 await s.page.keyboard.press('Enter');
 await s.page.keyboard.type('echo GUCODE-RETURNED');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('\nGUCODE-RETURNED\n'));
 await s.page.keyboard.type('exit');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>!shell.ui.busy&&!shell.busybox.running);
 assert.deepEqual(s.errors,[]);
 console.log('PASS public gucode 0.1.2 install/reload, Ctrl+A/E edits /exit, normal shell afterward; runtime '+await s.page.evaluate(()=>shell.toolchain.systemBuild));
}finally{await s.close();}
