import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
const ctx=vm.createContext({TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,Atomics,Uint8Array,DataView});
vm.runInContext(await readFile(new URL('../src/kernel.js',import.meta.url),'utf8'),ctx);
const Kernel=vm.runInContext('Kernel',ctx),k=new Kernel({createWorker(){throw Error("unexpected spawn");}});
const a=k.createTty({interactiveOut:true}),b=k.createTty({interactiveOut:true,makeDefault:false});
assert.equal(k._tty,a);
const fa=k._stdOfds(a),fb=k._stdOfds(b);
assert.notEqual(fa.in_.id,fb.in_.id);assert.equal(fa.in_.tty,a);assert.equal(fb.in_.tty,b);assert.equal(k._stdOfds(a),fa);
const seen=[];let pid=0;k._spawn=async(parent,spec)=>{const id=++pid;seen.push(spec);k._procs.set(id,{pgid:id});return {pid:id};};
const [pa,pb]=await Promise.all([k.boot({path:'/bin/sh',tty:a}),k.boot({path:'/bin/sh',tty:b})]);
assert.equal(seen[0].tty,a);assert.equal(seen[1].tty,b);assert.equal(a.fgPgid,pa);assert.equal(b.fgPgid,pb);assert.equal(k._tty,a);
const c=k.createTty({});assert.equal(k._tty,c); // preserve old default-replacement semantics
console.log('PASS separate tty stdio descriptors, concurrent boot tty selection and independent foreground process groups');
