import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({...(process.env.LIVE_CORE_URL?{url:process.env.LIVE_CORE_URL}:{}),timeout:240000});
s.page.setDefaultTimeout(30000);
try{
 if(process.env.EXPECTED_RUNTIME)assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),process.env.EXPECTED_RUNTIME);
 await s.command('true');
 await s.page.evaluate(source=>shell.busybox.write('/tmp/terminal-echo.c',new TextEncoder().encode(source)),await readFile(new URL('fixtures/terminal-echo.c',import.meta.url),'utf8'));
 console.log(await s.command('set -eu\ncc /tmp/terminal-echo.c -o /tmp/terminal-echo\n/tmp/terminal-echo'));
 const start=await s.page.evaluate(()=>shell.busybox.log.length);
 await s.page.locator('#terminal-focus').click();
 await s.page.locator('.xterm-helper-textarea').focus();
 await s.page.keyboard.type('sh');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(n=>shell.busybox.log.slice(n).includes('$ '),start);
 await s.page.keyboard.type('/tmp/terminal-echo interactive');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('ECHO-KEYBOARD-READY'));
 await s.page.keyboard.type('ab');await s.page.keyboard.press('ArrowLeft');
 await s.page.keyboard.press('Control+a');await s.page.keyboard.press('Control+e');
 await s.page.keyboard.type('Z');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('ab^[[D^A^EZ'));
 await s.page.waitForFunction(()=>{const b=shell.terminal.buffer.active;return Array.from({length:b.length},(_,i)=>b.getLine(i)?.translateToString(true)).includes('ab^[[D^A^EZ');});
 await s.page.locator('#startup').waitFor({state:'hidden'});
 const evidence=process.env.AGENT_EVIDENCE||'docs/research/terminal-echo-1.0.2';
 await mkdir(evidence,{recursive:true});
 await s.page.screenshot({path:evidence+'/' +(process.env.LIVE_CORE_URL?'public':'local')+'-echo.png'});
 await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>shell.busybox.log.includes('ECHO-KEYBOARD-PASS'));
 await s.page.keyboard.type('exit');await s.page.keyboard.press('Enter');
 await s.page.waitForFunction(()=>!shell.ui.busy&&!shell.busybox.running);
 assert.match(await s.command('echo SHELL-RECOVERED'),/SHELL-RECOVERED/);
 console.log('PASS guest PTY termios and real VT keyboard: visible controls, unchanged input bytes, normal shell afterward; runtime '+await s.page.evaluate(()=>shell.toolchain.systemBuild));
}catch(e){console.error(await s.page.evaluate(()=>({log:shell.busybox.log.slice(-3000),active:document.activeElement?.outerHTML,result:window.echoResult})));throw e;}
finally{await s.close();}
