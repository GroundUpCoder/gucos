import assert from 'node:assert/strict';
import {mkdtemp,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';
const dir=await mkdtemp(join(tmpdir(),'platform-metadata-'));
try{
 const bin=join(dir,'json'),file=join(dir,'manifest.json');
 const compile=spawnSync('cc',['-O0','tools/guest-repository/json.c','-o',bin],{encoding:'utf8'});assert.equal(compile.status,0,compile.stderr);
 const platform={target:'wasm32-wasm-posix',minVersion:'1.0.0',runtimeAbi:5,objectAbi:3,features:['sdl3-ime']};
 const args=['wasm32-wasm-posix','1.2.3','5','3',',posix-core,sdl3-ime,','legacy','current'];
 const run=async(value,operation='compatible',tail=args)=>{await writeFile(file,JSON.stringify(value));return spawnSync(bin,[operation,file,...tail],{encoding:'utf8'});};
 assert.equal((await run({platform})).status,0);
 for(const patch of [{minVersion:'2.0.0'},{minVersion:'1.2.4'},{minVersion:'1.02.0'},{minVersion:'1.0.0-rc.1'},{target:'wasm64'},{runtimeAbi:6},{objectAbi:4},{features:['tcp']}])assert.notEqual((await run({platform:{...platform,...patch}})).status,0,JSON.stringify(patch));
 assert.equal((await run({runtime:'legacy'})).status,0);assert.notEqual((await run({runtime:'other'})).status,0);
 const dep={platform,dependencyRequirements:{a:{minVersion:'1.1.0',maxExclusive:'2.0.0'}}};
 assert.equal((await run(dep,'dependency',['a','1.2.0','hash'])).status,0);
 for(const version of ['1.0.0','2.0.0'])assert.notEqual((await run(dep,'dependency',['a',version,'hash'])).status,0);
 assert.notEqual((await run(dep,'dependency',['b','1.2.0','hash'])).status,0);
 assert.equal((await run({platform,dependencyRequirements:{a:{version:'upstream-3',sha256:'hash'}}},'dependency',['a','upstream-3','hash'])).status,0);
 console.log('PASS stable SemVer, target/feature/ABI rejection, legacy baseline, dependency bounds and exact pins');
}finally{await rm(dir,{recursive:true,force:true});}
