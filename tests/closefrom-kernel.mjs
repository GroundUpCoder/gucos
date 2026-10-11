import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
const c=vm.createContext({console,TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,Atomics,Uint8Array,DataView});c.self=c;
for(const name of ['host','kernel','ipc-services'])vm.runInContext(await readFile(new URL('../src/'+name+'.js',import.meta.url),'utf8'),c);
const {Kernel,RemoteFS,OP}=c.KERNEL;
let releases=0;
const k=new Kernel({createWorker(){throw Error('unexpected');},fs:{close(){releases++;}}});
k._respond=(p,r)=>p.reply=r;
const p={pid:1,cid:1,fds:new Map(),fdFlags:new Map(),fdEpoch:new Int32Array(new SharedArrayBuffer(4))};
k._procs.set(1,p);
const o=k._makeOfd('file',{bfsFd:9});for(const fd of [4,2000000000]){k._ofdRef(o,1);p.fds.set(fd,o.id);p.fdFlags.set(fd,1);}
const thread=Object.create(p);
k._fsRpc(thread,OP.FS_CLOSEFROM,{lowfd:1000000});
assert(p.fds.has(4));assert(!p.fds.has(2000000000));assert(!p.fdFlags.has(2000000000));assert.equal(o.refs,1);
const r=new RemoteFS({call(op,args){k._fsRpc(p,op,args);return p.reply;}},{fdEpoch:p.fdEpoch.buffer});
r._fdTable[4]={readCache:{stale:true}};r._pipes.set(4,{stale:true});
k._closeFrom(thread,4);r._syncFdEpoch();assert.equal(Object.keys(r._fdTable).length,0);assert.equal(r._pipes.size,0);assert.equal(releases,1);
// Exercise a real sealed BlockFS image, whose descriptors are not in pcb.fds.
const image=c.BLOCK_FS.createV4(new c.BLOCK_FS.MemoryByteStore(1<<20));
const seed=image.open('/data',0x42,0o600);assert(seed>=0);
assert.equal(image.write(seed,new Uint8Array([7]),1),1);image.close(seed);image._readonly=true;
const local=new RemoteFS({call(op,args){k._fsRpc(p,op,args);return p.reply;}},{roFs:image,roPrefix:'/usr',roDev:2,roLeaf:true,fdEpoch:p.fdEpoch.buffer});
const lower=local.open('/usr/data',0,0),upper=local.open('/usr/data',0,0);
assert(lower>=1048576);assert(upper>lower);local.closefrom(upper);
assert.equal(local._ro.fds.has(upper),false);assert.equal(image.fstat(upper-1048576),null);
const byte=new Uint8Array(1);assert.equal(local.read(lower,byte,1),1);assert.equal(byte[0],7);
local.closefrom(0);assert.equal(local._ro.fds.size,0);
// Queued SCM_RIGHTS retains an OFD until the receive endpoint is closed.
k._sockRpc(p,OP.SOCK_PAIR,{type:1});const pair=p.reply;
const held=k._makeOfd('file',{bfsFd:10});k._ofdRef(held,1);p.fds.set(2000000000,held.id);
const sender=k._ofds.get(p.fds.get(pair.fd0));
k._sockEnqueue(sender.tx,1,[held],'');sender.tx.buf.push(42);
k._closeFrom(p,0);assert.equal(k._rightsQueued,0);assert.equal(k._ofds.size,0);assert.equal(releases,2);
const bfs=Object.create(c.BLOCK_FS.BlockFS.prototype);
bfs._fdTable=[];bfs._fdTable[0]={console:true};bfs._fdTable[2000000000]={console:true};
bfs.closefrom(1);assert(bfs._fdTable[0]);assert.equal(bfs._fdTable[2000000000],null);
bfs.closefrom(-1);assert.equal(bfs._fdTable[0],null);
console.log('CLOSEFROM-KERNEL-PASS: sparse maps, shared tables/cache invalidation, sealed-local keys, queued rights, console slots');
