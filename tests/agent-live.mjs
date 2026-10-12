// The test harness provisions a disposable guest disk. Runtime code reads only KEY_PATH in gucOS.
import assert from 'node:assert/strict';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {browserSession} from '../tools/browser.mjs';
const key=(await readFile(process.env.DEEPSEEK_TEST_KEY_FILE||homedir()+'/.guc/creds/deepseek-api-key','utf8')).trim();
if(!key)throw Error('Empty test credential');
const evidence=process.env.AGENT_EVIDENCE||'docs/research/agent-vt-1.0.3/live-provider';
const s=await browserSession({...(process.env.LIVE_CORE_URL?{url:process.env.LIVE_CORE_URL}:{}),allowedOrigins:['https://api.deepseek.com'],timeout:240000});
s.page.setDefaultTimeout(120000);
try{
 if(process.env.EXPECTED_RUNTIME)assert.equal(await s.page.evaluate(()=>shell.toolchain.systemBuild),process.env.EXPECTED_RUNTIME);
 await s.command('true');
 await s.page.evaluate(key=>shell.busybox.rpc('atomic-write',{path:'/home/user/.guc/creds/deepseek-api-key',bytes:new TextEncoder().encode(key),mode:0o600}),key);
 await s.page.locator('#new-agent').click();let f=s.page.frameLocator('iframe').first();await f.getByRole('button',{name:'New conversation',exact:true}).last().click();
 await s.page.waitForFunction(()=>document.querySelectorAll('iframe').length===2);f=s.page.frameLocator('iframe').last();
 const id=await s.page.locator('iframe').last().evaluate(e=>new URL(e.src).hash.slice(1));
 await f.getByTestId('agent-composer-input').fill('Use your Bash tool to run exactly: printf live-deepseek-proof > /tmp/agent-live-proof; cat /tmp/agent-live-proof . Then report the output briefly. Do not read any other files.');
 await f.getByRole('button',{name:'Send',exact:true}).click();
 let t;const states=[];const deadline=Date.now()+180000;
 do{
  t=await s.page.evaluate(async id=>JSON.parse(new TextDecoder().decode(await shell.busybox.read('/home/user/.guc/agent/threads/'+id+'.json'))),id);
  const state=t.status+':'+t.blocks.length;if(states.at(-1)!==state){states.push(state);console.log('Live turn',state);}
  if(t.blocks.length>1&&t.status!=='running')break;
  if(Date.now()>deadline)throw Error('Live turn timed out: '+states.join(', '));
  await s.page.waitForTimeout(300);
 }while(true);
 assert.equal(t.status,'idle',t.blocks.filter(b=>b.type==='info').map(b=>b.content).join('\n'));
 assert(t.blocks.some(b=>b.type==='toolUse'&&b.name==='Bash'));
 assert(t.blocks.some(b=>b.type==='toolResult'&&b.content.includes('live-deepseek-proof')));
 assert.equal(await s.page.evaluate(async()=>new TextDecoder().decode(await shell.busybox.read('/tmp/agent-live-proof'))),'live-deepseek-proof');
 assert(!JSON.stringify(t).includes(key));assert.deepEqual(s.errors,[]);
 await mkdir(evidence,{recursive:true});
 await writeFile(evidence+'/result.json',JSON.stringify({passed:true,runtime:await s.page.evaluate(()=>shell.toolchain.systemBuild),model:t.model,blocks:t.blocks.map(b=>({type:b.type,name:b.name,state:b.state})),usage:t.blocks.filter(b=>b.type==='usage'),scope:'Authenticated browser-to-DeepSeek SSE, real Bash tool writes and reads shared gucOS disk, assistant completion. Temporary guest credential never exported.'},null,2)+'\n');
 await s.page.screenshot({path:evidence+'/conversation.png'});
 console.log('PASS authenticated DeepSeek browser agent and actual gucOS Bash tool; model '+t.model);
}finally{await s.close();}
