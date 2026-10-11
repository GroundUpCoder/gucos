// Focused regression for transferred terminal ownership and descriptor sizing.
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFile} from 'node:fs/promises';
const ctx=vm.createContext({console,TextEncoder,TextDecoder,performance,setTimeout,clearTimeout,SharedArrayBuffer,Atomics,Uint8Array,DataView});
vm.runInContext(await readFile(new URL('../src/kernel.js',import.meta.url),'utf8'),ctx);
const {Kernel,Tty,OP,writePayload}=vm.runInContext('({Kernel,Tty,OP,writePayload})',ctx);
const k=new Kernel({createWorker(){throw Error('unexpected worker');},fs:{close(){}}});
const sab=new SharedArrayBuffer(65536);
const p={pid:2,cid:2,pgid:2,sid:2,tty:null,fds:new Map(),fdFlags:new Map(),sigdisp:[],i32:new Int32Array(sab),u8:new Uint8Array(sab),state:0};
k._procs.set(2,p);k._respond=(p,r)=>{p.result=r;};k._respondRaw=(p,r)=>{p.result={raw:r};};
let stopped=0;k._killPgid=()=>{stopped++;};
const tty=new Tty(k,{cols:117,rows:41});tty._brokered=true;tty.fgPgid=99;
const o=k._makeOfd('tty',{tty});k._ofdRef(o,2);p.fds.set(7,o.id);
// The server does not own this controlling terminal; immediate and deferred
// input both remain readable despite its different process group.
tty._cooked.push(65);k._fsRpc(p,OP.FS_READ,{fd:7,count:1});assert.equal(p.result.raw[0],65);
p.result=null;k._fsRpc(p,OP.FS_READ,{fd:7,count:1});assert.equal(p.waiter.op,'ttyread');
tty._cooked.push(66);k._ttyNotify(tty);assert.equal(p.result.raw[0],66);assert.equal(stopped,0);
// Owning the controlling terminal still enables the background-read check.
p.tty=tty;k._fsRpc(p,OP.FS_READ,{fd:7,count:1});assert.equal(p.result.errno,'EINTR');assert.equal(stopped,1);p.tty=null;
function size(fd){writePayload(p.i32,p.u8,{fd});Atomics.store(p.i32,4,1);Atomics.store(p.i32,5,OP.TIOCGWINSZ);k._dispatchRpc(p);return p.result;}
assert.equal(size(7).rows,41);assert.equal(size(7).cols,117);
const other=new Tty(k,{cols:63,rows:19});const master=k._makeOfd('ptm',{pty:{tty:other}});k._ofdRef(master,2);p.fds.set(8,master.id);
assert.equal(size(8).rows,19);assert.equal(size(8).cols,63);
assert.equal(size(9).errno,'EBADF');
const file=k._makeOfd('file',{});k._ofdRef(file,2);p.fds.set(9,file.id);assert.equal(size(9).errno,'ENOTTY');
console.log('PASS: detached terminal immediate/deferred reads, controlling-terminal guard, per-fd winsize');
