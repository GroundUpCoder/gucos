import {spawn} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
const mode=process.argv[2]||'local';
const base='docs/research/agent-vt-1.0.3/'+mode;
await mkdir(base,{recursive:true});
const tests=mode.startsWith('local')?['agent-state','terminal-vt-kernel','terminal-echo-kernel','vt-host-regressions','smoke','runtime-packages','agent-patch-upgrade','virtual-terminals','terminal-echo','agent-vt','agent-recovery','agent-live']:['agent-vt','agent-recovery','agent-live','live-core'];
for(const test of tests){
 let log='';
 const args=['tests/'+test+'.mjs',...(test==='live-core'?[process.env.EXPECTED_RUNTIME]:[])];
 console.log('TEST '+test);
 const child=spawn(process.execPath,args,{env:{...process.env,AGENT_EVIDENCE:base+'/'+test},stdio:['ignore','pipe','pipe']});
 for(const channel of [child.stdout,child.stderr])channel.on('data',bytes=>{const text=bytes.toString();log+=text;process.stdout.write(text);});
 const code=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',resolve);});
 await writeFile(base+'/'+test+'.log',log);
 if(code!==0)throw Error(test+' failed with exit '+code);
}
console.log('PASS '+mode+' acceptance suite');
