import assert from 'node:assert/strict';
import {pollPage} from './agent-helpers.mjs';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession({...(process.env.LIVE_CORE_URL?{url:process.env.LIVE_CORE_URL}:{}),timeout:240000});s.page.setDefaultTimeout(30000);
try{
 await s.command('true');
 await s.page.evaluate(()=>shell.busybox.rpc('atomic-write',{path:'/home/user/.guc/creds/deepseek-api-key',bytes:new TextEncoder().encode('test-key')}));
 await s.page.locator('#new-agent').click();await s.page.frameLocator('iframe').getByRole('button',{name:'New conversation',exact:true}).last().click();await s.page.waitForFunction(()=>document.querySelectorAll('iframe').length===2);
 let f=s.page.frameLocator('iframe').last();let calls=0;
 await s.page.route('https://api.deepseek.com/chat/completions',async route=>{
  calls++;const callId='call-'+calls;
  const data={choices:[{delta:{tool_calls:[{index:0,id:callId,function:{name:'Bash',arguments:JSON.stringify({command:'echo started >> /tmp/agent-recovery; sleep 30; echo finished >> /tmp/agent-recovery'})}}]},finish_reason:'tool_calls'}]};
  await route.fulfill({status:200,headers:{'access-control-allow-origin':'*'},contentType:'text/event-stream',body:'data: '+JSON.stringify(data)+'\n\ndata: [DONE]\n\n'});
 });
 const id=await s.page.locator('iframe').last().evaluate(e=>new URL(e.src).hash.slice(1));
 await f.getByTestId('agent-composer-input').fill('Run a long command');await f.getByTestId('agent-composer-send').click();
 await pollPage(s.page,async()=>new TextDecoder().decode(await shell.busybox.read('/tmp/agent-recovery')||new Uint8Array()).includes('started'));
 await f.getByRole('button',{name:'Stop',exact:true}).click();await f.getByText('Stopped by you.',{exact:true}).waitFor();
 assert.equal(calls,1);assert.equal(await s.command('cat /tmp/agent-recovery'),'started\n');
 await f.getByTestId('agent-composer-input').fill('Start another long command');await f.getByTestId('agent-composer-send').click();
 await pollPage(s.page,async()=>new TextDecoder().decode(await shell.busybox.read('/tmp/agent-recovery')||new Uint8Array()).split('started').length===3);
 await s.command('sync');
 assert.equal(await s.command('cat /tmp/agent-recovery'),'started\nstarted\n');
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);await s.command('true');await s.page.evaluate(id=>shell.openAgent(id),id);f=s.page.frameLocator('iframe');
 await f.getByText('This turn was interrupted. Saved output is restored; send a message to continue.',{exact:true}).waitFor();
 const t=await s.page.evaluate(async id=>JSON.parse(new TextDecoder().decode(await shell.busybox.read('/home/user/.guc/agent/threads/'+id+'.json'))),id);
 assert.equal(t.status,'interrupted');assert.equal(calls,2);assert(t.blocks.some(b=>b.type==='toolResult'&&b.content.includes('outcome is unknown')));assert.equal(await s.command('cat /tmp/agent-recovery'),'started\nstarted\n');
 // Restoring a second time is idempotent.
 await f.getByRole('button',{name:'Close VT',exact:true}).click();await s.page.evaluate(id=>shell.openAgent(id),id);await s.page.frameLocator('iframe').getByText('This turn was interrupted. Saved output is restored; send a message to continue.',{exact:true}).waitFor();
 assert.equal(calls,2);assert.deepEqual(s.errors,[]);
 console.log('PASS stop cancels actual process tree; reload restores durable intent/result, never repeats effects; repeated restore idempotent');
}finally{await s.close();}
