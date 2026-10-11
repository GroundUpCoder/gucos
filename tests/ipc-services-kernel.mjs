// Kernel lifecycle probes complement the compiled-C browser test: retained
// in-flight descriptions and unlinked object reclamation are not observable
// through a guest fd after close.
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
const c=vm.createContext({console,TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,Atomics});
c.self=c;
for(const name of ['kernel','ipc-services'])
  vm.runInContext(await readFile(new URL('../src/'+name+'.js',import.meta.url),'utf8'),c);
const k=new c.KERNEL.Kernel({createWorker(){throw Error('not used');},fs:{close(){}},fetch:null});
k._respond=(p,r)=>{p.reply=r;};k._respondRaw=(p,r)=>{p.reply=r;};
const proc=pid=>({pid,cid:pid,fds:new Map(),fdFlags:new Map(),waiter:null});
const p=proc(1), q=proc(2);k._procs.set(1,p);k._procs.set(2,q);
const svc=k._ipc;
svc.eventCreate(p,0,0);let fd=p.reply.fd,o=k._ofds.get(p.fds.get(fd));
svc.eventIO(p,o,null,8);assert(p.waiter);
k._ofdUnref(o.id,p.pid);p.fds.delete(fd);
assert.equal(o.refs,1);assert(k._ofds.has(o.id));
const bytes=new Uint8Array(8);new DataView(bytes.buffer).setBigUint64(0,23n,true);
svc.eventIO(q,o,bytes,8);
assert.equal(new DataView(p.reply.buffer).getBigUint64(0,true),23n);
assert.equal(p.waiter,null);assert(!k._ofds.has(o.id));

// Canceled reads and writes must never consume tokens or mutate the counter.
svc.eventCreate(p,0,0);fd=p.reply.fd;o=k._ofds.get(p.fds.get(fd));
svc.eventIO(p,o,null,8);k._cancelWaiter(p);
svc.eventIO(q,o,bytes,8);assert.equal(o.value,23n);assert.equal(o.waiters.size,0);
o.value=0xfffffffffffffffen;new DataView(bytes.buffer).setBigUint64(0,1n,true);
svc.eventIO(p,o,bytes,8);assert(p.waiter);k._cancelWaiter(p);
svc.eventIO(q,o,null,8);assert.equal(o.value,0n);assert.equal(o.refs,1);
k._ofdUnref(o.id,p.pid);p.fds.delete(fd);

// A waiting flock holds its OFD even after the fd closes; cancellation drops
// exactly that reference. Distinct opens (even in one pid) remain competitors.
const a=k._makeOfd('file',{ino:7,bfsFd:1}), b=k._makeOfd('file',{ino:7,bfsFd:2});
k._ofdRef(a,1);k._ofdRef(b,2);p.fds.set(3,a.id);q.fds.set(3,b.id);
svc.flock(p,3,2);svc.flock(q,3,2);assert(q.waiter);
k._ofdUnref(b.id,2);q.fds.delete(3);assert.equal(b.refs,1);
k._cancelWaiter(q);assert(!k._ofds.has(b.id));
svc.flock(p,3,8);assert.equal(svc.locks.size,0);
k._ofdUnref(a.id,1);p.fds.delete(3);

// Unlink leaves live handles usable. Fork inherits counts; exec/exit release
// all handles. An open of the same name after unlink gets a fresh identity.
svc.sem(p,{action:0,name:'/life',flags:0xc0,mode:0o600,value:0});
const old=p.reply.id;svc.clone(1,2);
svc.sem(p,{action:2,name:'/life'});svc.release(1);assert(svc.sems.has(old));
svc.sem(p,{action:0,name:'/life',flags:0xc0,mode:0o600,value:5});
const fresh=p.reply.id;assert.notEqual(fresh,old);
svc.sem(q,{action:3,id:old});assert.equal(svc.sems.get(old).value,1);
svc.release(2);assert(!svc.sems.has(old));assert(svc.sems.has(fresh));
svc.sem(p,{action:2,name:'/life'});svc.release(1);
assert.equal(svc.sems.size,0);assert.equal(svc.names.size,0);assert.equal(svc.handles.size,0);
console.log('IPC-KERNEL-PASS: in-flight OFDs, cancellation, unlink and handle reclamation');
