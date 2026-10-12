import assert from 'node:assert/strict';
import {AgentRun,ThreadStore,newThread,apiMessages,streamEvents,KEY_PATH} from '../apps/agent-ui/agent.mjs';
const enc=new TextEncoder(),dec=new TextDecoder();
const disk=new Map([[KEY_PATH,enc.encode('fixture-secret')]]);let effects=0;
const session={read:async p=>disk.get(p),write:async(p,b)=>disk.set(p,b),rpc:async(op,a)=>{if(op==='atomic-write'){disk.set(a.path,a.bytes);return;}if(op==='exec'){effects++;return {output:'file written',code:0,signal:0};}if(op==='list-directory')return [...disk.keys()].filter(p=>p.startsWith(a.path+'/')).map(p=>p.split('/').at(-1));}};
const packets=data=>'data: '+JSON.stringify(data)+'\r\n\r\n';
const streamed=items=>new Response(new ReadableStream({start(c){for(const b of enc.encode(items.map(packets).join('')+'data: [DONE]\r\n\r\n'))c.enqueue(new Uint8Array([b]));c.close();}}));
let count=0;
const fetcher=async(url,opts)=>{const body=JSON.parse(opts.body);assert(!JSON.stringify(body).includes('fixture-secret'));assert.equal(opts.headers.Authorization,'Bearer fixture-secret');count++;
 if(count===1)return streamed([{choices:[{delta:{reasoning_content:'thinking 日本語'}}]},{choices:[{delta:{tool_calls:[{index:0,id:'c1',function:{name:'Bash',arguments:'{"command":'}}]}}]},{choices:[{delta:{tool_calls:[{index:0,function:{arguments:'"echo done"}'}}]},finish_reason:'tool_calls'}]}]);
 assert.equal(body.messages.at(-1).role,'tool');return streamed([{choices:[{delta:{content:'Done ✓'}}]},{choices:[{delta:{},finish_reason:'stop'}]},{choices:[],usage:{prompt_tokens:10,completion_tokens:5,prompt_tokens_details:{cached_tokens:2}}}]);};
const t=newThread(),store=new ThreadStore(session),run=new AgentRun(t,store,()=>{},fetcher);await run.send('Do something');
assert.equal(effects,1);assert.equal(t.status,'idle');assert.equal(count,2);assert.equal(t.blocks.find(b=>b.type==='thinking').content,'thinking 日本語');assert.equal(t.blocks.find(b=>b.type==='text'&&b.role==='assistant').content,'Done ✓');assert.equal(t.blocks.at(-1).cached_tokens,2);assert(t.blocks.every(b=>Object.values(b).every(v=>v==null||typeof v!=='object')));
const restored=await store.load(t.id);assert.deepEqual(apiMessages(restored),apiMessages(t));assert(!JSON.stringify(restored).includes('fixture-secret'));assert(!restored.messages);
// A durable intent with no result is never replayed after reload.
restored.status='running';restored.blocks.push({id:'pending',role:'assistant',type:'toolUse',messageId:'pending-message',callId:'pending-call',name:'Bash',input:'{"command":"dangerous-to-repeat"}',state:'complete',execution:'started'});
const recovery=new AgentRun(restored,store);await recovery.recover();assert.equal(effects,1);assert.equal(restored.status,'interrupted');assert.match(apiMessages(restored).at(-1).content,/outcome is unknown/);
// Incomplete streamed tool arguments are retained for display but never executed/sent.
const partial=newThread();const interrupted=new AgentRun(partial,store,()=>{},async()=>new Response('data: '+JSON.stringify({choices:[{delta:{tool_calls:[{index:0,id:'unfinished',function:{name:'Bash',arguments:'{"command":'}}]}}]})+'\n\n'));
await interrupted.send('Fail halfway');assert.equal(partial.status,'error');assert.equal(effects,1);assert(!apiMessages(partial).some(m=>m.tool_calls));
// Ordered writes are snapshots, not references; failed writes do not poison later saves.
let fail=true;const failing=new ThreadStore({...session,rpc:async(op,a)=>{if(fail){fail=false;throw Error('disk full');}return session.rpc(op,a);}});await assert.rejects(failing.save(t),/disk full/);await failing.save(t);
await assert.rejects(new ThreadStore({...session,persistence:'memory'}).save(t),/Persistent storage is unavailable/);
const failedTool=new AgentRun(newThread(),new ThreadStore({...session,rpc:async()=>({output:'failed command',code:2,signal:0})}));
await assert.rejects(failedTool.execute({name:'Bash',input:'{\"command\":\"false\"}'}),/failed command[\s\S]*exit 2/);
// Bytewise SSE handles multibyte UTF-8 and CRLF event boundaries.
const events=[];for await(const e of streamEvents(streamed([{text:'界🙂'}]).body))events.push(e);assert.equal(events[0].text,'界🙂');
console.log('PASS flat store, ordered durable writes, bytewise SSE, tool round trip, interrupted restore without replay, truncated calls, secret exclusion');
