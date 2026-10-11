import {spawnSync} from 'node:child_process';
import {writeFileSync,readFileSync} from 'node:fs';
const out='docs/research/terminal-echo-1.0.2',results=[];
for(const name of ['terminal-echo','terminal-patch-upgrade','ipc-sockets','ipc-services','ipc-terminal','ipc-symbols','closefrom','platform-posix','smoke','runtime-packages','storage-boundary','virtual-terminals','webgpu-vt']){
 console.log('Running '+name);const t=Date.now();
 const r=spawnSync(process.execPath,['tests/'+name+'.mjs'],{encoding:'utf8',timeout:600000,maxBuffer:16*1024*1024});
 writeFileSync(out+'/'+name+'.log',(r.stdout||'')+(r.stderr||''));results.push({name,status:r.status,signal:r.signal,seconds:(Date.now()-t)/1000});
 writeFileSync(out+'/gates.json',JSON.stringify({runtime:JSON.parse(readFileSync('dist/build.json')).runtime,results},null,2)+'\n');
 console.log(JSON.stringify(results.at(-1)));if(r.status!==0)process.exit(1);
}
