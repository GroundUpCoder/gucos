import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
import {runCompilerScript} from '../../../tools/compiler-context.mjs';
const root=new URL('../../../',import.meta.url);
const c=vm.createContext({console,TextEncoder,TextDecoder,performance,WebAssembly,decoder:new TextDecoder(),
 readFile:()=>undefined,writeFile:()=>{},snapshot:()=>({}),systemFS:{stat:()=>null,chmod(){},_s:{flush(){}}}});
c.self=c;
for(const name of ['path','compiler','trampoline','objects','compile','compiler-driver'])
 runCompilerScript(await readFile(new URL('src/'+name+'.js',root),'utf8'),c,{filename:name+'.js'});
for(const [path,fork] of [['tests/fixtures/ipc-sockets.c',true],['docs/research/unix-socket-ipc/adapted-probes.c',false]]){
 const errors=[],source=await readFile(new URL(path,root),'utf8');
 const result=c.compile({'/probe.c':source},['/probe.c'],[],s=>errors.push(s),{compilerOptions:{enableFork:fork}});
 if(!WebAssembly.validate(result.bytes))throw Error(path+': invalid Wasm');
 console.log(path+': compiled and validated '+result.bytes.length+' Wasm bytes; enableFork='+fork);
 if(errors.length)console.log(errors.join(''));
}

