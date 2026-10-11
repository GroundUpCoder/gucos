// Handcrafted lifecycle tests: use the real socket RPC and OFD machinery.
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
const ctx=vm.createContext({console,TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,Atomics,Uint8Array,DataView});
vm.runInContext(await readFile(new URL('../src/kernel.js',import.meta.url),'utf8'),ctx);
const {Kernel,OP}=vm.runInContext('({Kernel,OP})',ctx);
let closed=0;
const k=new Kernel({createWorker(){throw Error('unexpected worker');},fs:{close(){closed++;}}});
k._respond=(p,r)=>{p.result=r;};k._respondRaw=(p,r)=>{p.result={raw:r};};
k._deliver=(p,s)=>{p.signal=s;};
const proc=id=>{const p={pid:id,cid:id,fds:new Map(),fdFlags:new Map(),state:0,waiter:null};k._procs.set(id,p);return p;};
const p=proc(1),q=proc(2);
const rpc=(p,op,r)=>{p.result=null;k._sockRpc(p,op,r);return p.result;};
const pair=(type=1)=>{const r=rpc(p,OP.SOCK_PAIR,{type});return [r.fd0,r.fd1];};
const ofd=fd=>k._ofds.get(p.fds.get(fd));
const close=(fd,who=p)=>{k._fsRpc(who,OP.FS_CLOSE,{fd});};
const file=()=>{const o=k._makeOfd('file',{bfsFd:99});k._ofdRef(o,p.pid);let fd=0;while(p.fds.has(fd))fd++;p.fds.set(fd,o.id);return fd;};
const send=(fd,rights=[],bytes=[42],flags=0,who=p)=>{
 const b=new Uint8Array(16+4*rights.length+bytes.length),v=new DataView(b.buffer);
 v.setInt32(0,fd,true);v.setInt32(4,flags,true);v.setInt32(8,rights.length,true);
 rights.forEach((f,i)=>v.setInt32(16+i*4,f,true));b.set(bytes,16+4*rights.length);
 return rpc(who,OP.SOCK_SENDMSG,{raw:b});
};
const recv=(fd,count=100,cap=253,flags=0)=>{
 const r=rpc(p,OP.SOCK_RECVMSG,{fd,count,rights:cap,flags});if(!r||!r.raw)return r;
 const v=new DataView(r.raw.buffer,r.raw.byteOffset,r.raw.byteLength),nf=v.getUint32(8,true),names=v.getUint32(12,true);
 return {n:v.getUint32(0,true),flags:v.getUint32(4,true),fds:Array.from({length:nf},(_,i)=>v.getInt32(16+i*4,true)),bytes:Array.from(r.raw.slice(16+nf*4+names))};
};
{
 const [a,b]=pair(),f=file(),o=ofd(f);assert.equal(send(a,[f]).n,1);close(f);
 assert.equal(o.refs,1);assert.equal(closed,0);
 const peek=recv(b,1,1,2|0x40000000);assert.equal(p.fdFlags.get(peek.fds[0]),1);assert.equal(o.refs,2);close(peek.fds[0]);
 const got=recv(b);assert.equal(ofd(got.fds[0]),o);assert.equal(p.fdFlags.get(got.fds[0]),undefined);close(got.fds[0]);
 assert.equal(closed,1);close(a);close(b);
}
{
 const [a,b]=pair(),f=file(),o=ofd(f);send(a,[f,f]);close(f);
 const got=recv(b,1,1);assert.equal(got.flags,8);assert.equal(o.refs,1);close(got.fds[0]);close(a);close(b);
}
{
 const [a,b]=pair(),f=file(),o=ofd(f);send(a,[f]);close(f);
 k._fsRpc(p,OP.FS_READ,{fd:b,count:1});assert.equal(k._ofds.has(o.id),false);close(a);close(b);
}
{
 const [a,b]=pair(),f=file(),o=ofd(f);send(a,[f]);close(f);
 rpc(p,OP.SOCK_SHUTDOWN,{fd:b,how:0});assert.equal(k._ofds.has(o.id),true);
 const got=recv(b);close(got.fds[0]);assert.equal(k._ofds.has(o.id),false);close(a);close(b);
}
// A cycle across two receive queues with no outside root.
{
 const [a,b]=pair(),[c,d]=pair();send(a,[c]);send(d,[b]);
 close(a);close(d);close(b);close(c);
 assert.equal(k._ofds.size,0);assert.equal(k._rightsQueued,0);
}
// A self-reference and a file must both be reclaimed.
{
 const [a,b]=pair(),f=file();send(a,[b,f]);close(f);close(a);close(b);assert.equal(k._ofds.size,0);
}
// Full queue: no refs acquired on EAGAIN; parked send owns refs until cancel.
{
 const [a,b]=pair(),tx=ofd(a).tx;tx.cap=1;send(a);
 const f=file(),o=ofd(f);assert.equal(send(a,[f],[8],0x40).errno,'EAGAIN');assert.equal(o.refs,1);
 send(a,[f]);assert.equal(p.waiter.op,'pipewrite');assert.equal(o.refs,2);close(f);k._cancelWaiter(p);
 assert.equal(k._ofds.has(o.id),false);close(a);close(b);
}
// Pending send commits references before dropping the waiter's ownership.
{
 const [a,b]=pair(),tx=ofd(a).tx;tx.cap=1;send(a);
 const f=file(),o=ofd(f);q.fds.set(0,p.fds.get(a));k._ofdRef(ofd(a),q.pid);q.fds.set(1,o.id);k._ofdRef(o,q.pid);
 send(0,[1],[9],0,q);close(f);close(1,q);
 recv(b);assert.equal(q.result.n,1);assert.equal(o.refs,1);const got=recv(b);assert.equal(got.bytes[0],9);close(got.fds[0]);
 close(0,q);close(a);close(b);
}
// A blocked operation retains its socket after another thread closes the fd.
{
 const [a,b]=pair(),receiver=ofd(b);
 rpc(p,OP.SOCK_RECVMSG,{fd:b,count:1,rights:0,flags:0});close(b);
 assert(k._ofds.has(receiver.id));q.fds.set(0,p.fds.get(a));k._ofdRef(ofd(a),q.pid);
 send(0,[],[77],0,q);assert.equal(p.result.raw[16],77);assert.equal(k._ofds.has(receiver.id),false);
 close(0,q);close(a);
}
{
 const [a,b]=pair(),sender=ofd(a);sender.tx.cap=1;send(a);
 q.fds.set(0,p.fds.get(a));k._ofdRef(sender,q.pid);send(0,[],[78],0,q);close(0,q);close(a);
 assert(k._ofds.has(sender.id));recv(b);assert.equal(q.result.n,1);
 assert.equal(recv(b).bytes[0],78);assert.equal(k._ofds.has(sender.id),false);close(b);
}
for(const type of [2,5]){
 const [a,b]=pair(type),f=file(),o=ofd(f);send(a,[f],[1,2,3]);close(f);
 const got=recv(b,1,0,0x20);assert.equal(got.n,3);assert.equal(got.flags,0x28);assert.deepEqual(got.bytes,[1]);assert.equal(k._ofds.has(o.id),false);
 assert.equal(recv(b,10,0,0x40).errno,'EAGAIN');
 send(a,[],[]);assert(k._pollState(p,b)&1);assert.equal(recv(b,0,0).n,0);assert.equal(recv(b,1,0,0x40).errno,'EAGAIN');
 close(a);close(b);
}
// Datagram SHUT_WR is per sender, not a shutdown of the destination queue.
{
 const [a,b]=pair(2),target=ofd(b).rx;
 rpc(p,OP.SOCK_SHUTDOWN,{fd:a,how:1});assert.equal(target.wOpen,true);
 assert.equal(send(a,[],[1],0x4000).errno,'EPIPE');close(a);close(b);
}
// Closing a datagram peer disconnects the sender after ECONNREFUSED.
{
 const [a,b]=pair(2);close(b);
 assert.equal(send(a,[],[1],0x4000).errno,'ECONNREFUSED');
 assert.equal(send(a,[],[1],0x4000).errno,'ENOTCONN');close(a);
}
// Empty packet queues are bounded independently of their byte length.
{
 const [a,b]=pair(2);
 for(let i=0;i<256;i++)assert.equal(send(a,[],[],0x40).n,0);
 assert.equal(send(a,[],[],0x40).errno,'EAGAIN');recv(b,0,0);
 assert.equal(send(a,[],[],0x40).n,0);close(a);close(b);
}
assert.equal(k._ofds.size,0);assert.equal(k._rightsQueued,0);
console.log('IPC-SOCKET-KERNEL-PASS');

