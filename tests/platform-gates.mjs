import {spawnSync} from 'node:child_process';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
const build=JSON.parse(await readFile('dist/build.json','utf8'));
const results=[];let log='Candidate '+build.runtime+' platform '+build.platform.version+'\n';
const tests=process.argv.slice(2);
for(const name of tests.length?tests:['platform-posix','platform-ime','startup-progress','platform-upgrade','smoke','runtime-packages','storage-boundary','virtual-terminals','webgpu-vt','vt-host-regressions']){
 const start=Date.now();console.log('Running '+name);
 const r=spawnSync(process.execPath,['tests/'+name+'.mjs'],{encoding:'utf8',timeout:600000,maxBuffer:16*1024*1024});
 const entry={name,status:r.status,signal:r.signal,seconds:(Date.now()-start)/1000};results.push(entry);
 log+='\n'+JSON.stringify(entry)+'\n'+(r.stdout||'')+(r.stderr||'');console.log(r.stdout||'');if(r.stderr)console.error(r.stderr);
 await mkdir('docs/research/platform-1.0',{recursive:true});
 await writeFile('docs/research/platform-1.0/core-gates.log',log);
 await writeFile('docs/research/platform-1.0/core-gates.json',JSON.stringify({runtime:build.runtime,platform:build.platform,results},null,2)+'\n');
 if(r.status!==0)process.exit(1);
}
