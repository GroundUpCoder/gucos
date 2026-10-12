export const THREAD_DIR='/home/user/.guc/agent/threads';
export const KEY_PATH='/home/user/.guc/creds/deepseek-api-key';
export const API_URL='https://api.deepseek.com/chat/completions';
export const DEFAULT_MODEL='deepseek-flash';
const enc=new TextEncoder(),dec=new TextDecoder();
const uid=()=>crypto.randomUUID();
export const tools=[
  ['Bash','Run a shell command in gucOS. Each command starts in the thread working directory. Use cd in the command as needed. Processes and files are on the shared gucOS machine.',{command:{type:'string'},timeout:{type:'integer'}},['command']],
  ['Read','Read a UTF-8 file in gucOS.',{file_path:{type:'string'}},['file_path']],
  ['Write','Write a UTF-8 file in gucOS, creating parent directories.',{file_path:{type:'string'},content:{type:'string'}},['file_path','content']],
  ['Edit','Replace an exact unique string in a UTF-8 file.',{file_path:{type:'string'},old_string:{type:'string'},new_string:{type:'string'}},['file_path','old_string','new_string']],
].map(([name,description,properties,required])=>({type:'function',function:{name,description,parameters:{type:'object',properties,required,additionalProperties:false}}}));
export function newThread(){return {format:1,id:uid(),title:'New conversation',createdAt:Date.now(),updatedAt:Date.now(),model:DEFAULT_MODEL,cwd:'/home/user',status:'idle',draft:'',blocks:[]};}
export class ThreadStore{
 constructor(session){this.session=session;this.queue=Promise.resolve();}
 async list(){const names=await this.session.rpc('list-directory',{path:THREAD_DIR});const rows=[];for(const name of names.filter(n=>/^[a-f0-9-]+\.json$/.test(n))){try{rows.push(await this.load(name.slice(0,-5)));}catch(error){rows.push({id:name.slice(0,-5),title:'Unreadable conversation',error:error.message,updatedAt:0});}}return rows.sort((a,b)=>b.updatedAt-a.updatedAt);}
 async load(id){if(!/^[a-f0-9-]+$/.test(id))throw Error('Invalid conversation ID');const bytes=await this.session.read(THREAD_DIR+'/'+id+'.json');if(!bytes)throw Error('Conversation not found');const t=JSON.parse(dec.decode(bytes));if(t.format!==1||!Array.isArray(t.blocks)||t.id!==id)throw Error('Unsupported conversation format; original file preserved.');return t;}
 save(t){if(this.session.persistence==='memory')return Promise.reject(Error('Persistent storage is unavailable. Agent conversations cannot be saved in this session.'));t.updatedAt=Date.now();const bytes=enc.encode(JSON.stringify(t)+'\n');const write=()=>this.session.rpc('atomic-write',{path:THREAD_DIR+'/'+t.id+'.json',bytes});const next=this.queue.then(write);this.queue=next.catch(()=>{});return next;}
}
export function apiMessages(t){
 const result=[{role:'system',content:'You are the gucOS agent. Help the user operate this browser OS using your tools. Execute tasks, inspect actual results, and report accurately. Files and processes are in gucOS, never the host computer. HOME is /home/user. Never read or disclose ~/.guc/creds or credentials. Do not claim actions without successful tools. Do not repeat interrupted actions without checking their effects. Use relative Markdown links for gucOS files. Working directory: '+t.cwd}];
 let current=null;
 for(const b of t.blocks){
  if(b.role==='user'){
   if(!current||current._id!==b.messageId){current={role:'user',content:[],_id:b.messageId};result.push(current);}
   if(b.type==='text')current.content.push({type:'text',text:b.content});
   if(b.type==='image')current.content.push({type:'image_url',image_url:{url:'data:'+b.mediaType+';base64,'+b.base64}});
  }else if(b.type==='toolResult'){
   result.push({role:'tool',tool_call_id:b.callId,content:b.content});current=null;
  }else if(b.role==='assistant'&&(b.state!=='incomplete'||b.type==='text')&&['text','thinking','toolUse'].includes(b.type)){
   if(!current||current._id!==b.messageId){current={role:'assistant',content:'',_id:b.messageId};result.push(current);}
   if(b.type==='text')current.content+=b.content;
   if(b.type==='thinking')current.reasoning_content=(current.reasoning_content||'')+b.content;
   if(b.type==='toolUse'){(current.tool_calls??=[]).push({id:b.callId,type:'function',function:{name:b.name,arguments:b.input}});}
  }
 }
 return result.map(({_id,...m})=>m);
}
// SSE framing is independent of network chunk and UTF-8 boundaries.
export async function* streamEvents(body){
 const reader=body.getReader(),decoder=new TextDecoder();let pending='';
 try{for(;;){const {value,done}=await reader.read();pending+=done?decoder.decode():decoder.decode(value,{stream:true});let match;
  while((match=/\r?\n\r?\n/.exec(pending))){const packet=pending.slice(0,match.index);pending=pending.slice(match.index+match[0].length);const data=packet.split(/\r?\n/).filter(l=>l.startsWith('data:')).map(l=>l.slice(5).trimStart()).join('\n');if(data==='[DONE]')return;if(data)yield JSON.parse(data);}
  if(done){if(pending.trim())throw Error('Incomplete response stream');return;}
 }}finally{reader.releaseLock();}
}
export class AgentRun{
 constructor(thread,store,changed=()=>{},fetcher=(...args)=>fetch(...args)){Object.assign(this,{thread,store,changed,fetcher});this.abort=null;this.execId=null;}
 block(value){const b={id:uid(),createdAt:Date.now(),...((value.role==='assistant'||value.role==='tool'||value.type==='usage')&&this.turnId?{turnId:this.turnId}:{}),...value};this.thread.blocks.push(b);return b;}
 async checkpoint(){await this.store.save(this.thread);this.changed();}
 async recover(){const t=this.thread;if(t.status!=='running')return;
  for(const b of t.blocks.filter(b=>b.type==='toolUse'&&b.state!=='incomplete'))if(!t.blocks.some(r=>r.type==='toolResult'&&r.callId===b.callId&&r.messageId===b.messageId))this.block({role:'tool',type:'toolResult',messageId:b.messageId,turnId:b.turnId,callId:b.callId,isError:true,content:'Interrupted by reload. Execution outcome is unknown. Inspect effects before retrying.'});
  t.status='interrupted';this.block({role:'system',type:'info',content:'This turn was interrupted. Saved output is restored; send a message to continue.'});await this.checkpoint();
 }
 stop(){this.abort?.abort();if(this.execId!=null)this.store.session.rpc('exec-stop',{id:this.execId}).catch(()=>{});}
 async execute(b){const a=JSON.parse(b.input),s=this.store.session;
  if(b.name==='Bash'){
   if(typeof a.command!=='string'||!a.command.trim())throw Error('command must be a nonempty string');
   const p=s.rpc('exec',{command:a.command,cwd:this.thread.cwd,timeout:a.timeout});this.execId=p.requestId;
   try{const r=await p;const output=r.output+'\n[exit '+r.code+(r.signal?', signal '+r.signal:'')+']';if(r.code||r.signal)throw Error(output);return output;}finally{this.execId=null;}
  }
  if(!['Read','Write','Edit'].includes(b.name))throw Error('Unknown tool '+b.name);
  if(typeof a.file_path!=='string')throw Error('file_path must be a string');
  const path=new URL(a.file_path.replace(/^~\//,'/home/user/'),'https://gucos'+this.thread.cwd+'/').pathname;
  if(path==='/home/user/.guc/creds'||path.startsWith('/home/user/.guc/creds/'))throw Error('Credential files are private.');
  if(b.name==='Read'){const bytes=await s.read(path);if(!bytes)throw Error('File not found: '+path);if(bytes.length>1000000)throw Error('File exceeds 1 MB; use Bash to inspect a range.');return dec.decode(bytes);}
  let content=a.content;
  if(b.name==='Edit'){const bytes=await s.read(path);if(!bytes)throw Error('File not found: '+path);const old=dec.decode(bytes);if(typeof a.old_string!=='string'||!a.old_string||typeof a.new_string!=='string')throw Error('Provide old_string and new_string');if(old.split(a.old_string).length!==2)throw Error('old_string must occur exactly once');content=old.replace(a.old_string,a.new_string);}
  if(typeof content!=='string')throw Error('content must be a string');await s.write(path,enc.encode(content));return 'Saved '+path;
 }
 async send(content){
  if(this.abort||this.starting)throw Error('A turn is already running');
  this.starting=true;let key;
  try{key=dec.decode(await this.store.session.read(KEY_PATH)||new Uint8Array()).trim();}finally{this.starting=false;}
  if(!key)throw Error('Add your DeepSeek API key at ~/.guc/creds/deepseek-api-key in gucOS, or use Agent settings.');
  const t=this.thread,messageId=uid();this.turnId=uid();this.abort=new AbortController();
  const userBlocks=typeof content==='string'?[{type:'text',text:content}]:content;
  if(!Array.isArray(userBlocks)||!userBlocks.length){this.abort=null;throw Error('Enter a message');}
  if(userBlocks.some(b=>!['text','image'].includes(b.type))){this.abort=null;throw Error('This provider accepts text and images.');}
  for(const b of userBlocks)this.block({role:'user',messageId,type:b.type,...(b.type==='text'?{content:b.text}:{mediaType:b.source?.media_type||b.mediaType,base64:b.source?.data||b.base64})});
  if(t.title==='New conversation')t.title=(userBlocks.find(b=>b.type==='text')?.text||'Image conversation').slice(0,70);
  t.status='running';t.draft='';
  try{
   await this.checkpoint();
   for(let step=0;step<50;step++){
    if(this.abort.signal.aborted)throw new DOMException('Stopped','AbortError');
    const messages=apiMessages(t),mid=uid(),calls=new Map();let textBlock,thinkingBlock,finish=null,lastSave=0;
    const response=await this.fetcher(API_URL,{method:'POST',headers:{'Content-Type':'application/json',Authorization:'Bearer '+key},body:JSON.stringify({model:t.model,messages,tools,stream:true,stream_options:{include_usage:true},max_tokens:8192}),signal:this.abort.signal});
    if(!response.ok){const body=await response.text();throw Error('DeepSeek HTTP '+response.status+': '+body.slice(0,500).replaceAll(key,'[redacted]'));}
    if(!response.body)throw Error('DeepSeek returned an empty response');
    for await(const e of streamEvents(response.body)){
     if(e.error)throw Error(e.error.message||'Provider error');
     if(e.usage)this.block({role:'system',type:'usage',messageId:mid,model:e.model||t.model,prompt_tokens:e.usage.prompt_tokens,completion_tokens:e.usage.completion_tokens,total_tokens:e.usage.total_tokens,cached_tokens:e.usage.prompt_tokens_details?.cached_tokens,reasoning_tokens:e.usage.completion_tokens_details?.reasoning_tokens,provider_usage:JSON.stringify(e.usage)});
     const choice=e.choices?.[0];if(!choice)continue;const d=choice.delta||{};
     if(d.reasoning_content){thinkingBlock??=this.block({role:'assistant',messageId:mid,type:'thinking',content:'',state:'incomplete'});thinkingBlock.content+=d.reasoning_content;}
     if(d.content){textBlock??=this.block({role:'assistant',messageId:mid,type:'text',content:'',state:'incomplete'});textBlock.content+=d.content;}
     for(const c of d.tool_calls||[]){let b=calls.get(c.index);if(!b){b=this.block({role:'assistant',messageId:mid,type:'toolUse',callId:'',name:'',input:'',state:'incomplete'});calls.set(c.index,b);}if(c.id)b.callId=c.id;if(c.function?.name)b.name+=c.function.name;if(c.function?.arguments)b.input+=c.function.arguments;}
     if(choice.finish_reason)finish=choice.finish_reason;
     this.changed();if(Date.now()-lastSave>300){await this.checkpoint();lastSave=Date.now();}
    }
    if(!finish)throw Error('Connection ended before the response completed');
    if(finish==='length')throw Error('Response reached its token limit. Send a follow-up to continue.');
    for(const b of [thinkingBlock,textBlock,...calls.values()].filter(Boolean))b.state='complete';
    await this.checkpoint();
    if(!calls.size){t.status='idle';await this.checkpoint();return;}
    for(const b of calls.values()){
     if(this.abort.signal.aborted)throw new DOMException('Stopped','AbortError');
     // Intent is durable before the first side effect. Never replay on restore.
     b.execution='started';await this.checkpoint();let output,isError=false;
     try{output=await this.execute(b);}catch(e){output=e.message;isError=true;}
     this.block({role:'tool',messageId:mid,type:'toolResult',callId:b.callId,content:output,isError});b.execution='finished';await this.checkpoint();
    }
   }
   throw Error('Turn reached 50 steps. Send a message to continue.');
  }catch(e){
   for(const b of t.blocks.filter(b=>b.type==='toolUse'&&b.state==='complete'))if(!t.blocks.some(r=>r.type==='toolResult'&&r.callId===b.callId&&r.messageId===b.messageId))this.block({role:'tool',messageId:b.messageId,type:'toolResult',callId:b.callId,isError:true,content:'Turn stopped before a result was recorded. Inspect effects before retrying.'});
   t.status=this.abort.signal.aborted?'stopped':'error';this.block({role:'system',type:'info',content:this.abort.signal.aborted?'Stopped by you.':e.message.replaceAll(key,'[redacted]')});await this.checkpoint();
  }finally{this.abort=null;this.changed();}
 }
}
