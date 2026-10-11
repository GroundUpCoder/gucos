// Broker-owned IPC. No application memory mappings; all mutations run in one
// kernel turn. Resource-local queues avoid scanning unrelated processes.
self.IPCServices = class IPCServices {
  constructor(kernel) {
    this.k = kernel;
    this.locks = new Map();
    this.names = new Map();
    this.sems = new Map();
    this.handles = new Map(); // pid -> (object id -> open count)
    this.nextSem = 1;
  }
  park(p, queue, fields) {
    const w = {op:'ipc', p, queue, ...fields};
    p.waiter = w; queue.add(w);
    if(w.heldOfd)this.k._ofdRef(w.heldOfd);
    return w;
  }
  cancel(w) {
    w.queue.delete(w);
    if(w.heldOfd)this.k._ofdUnref(w.heldOfd.id);
    if(w.sem)this.collect(w.sem);
  }
  finish(w, result, raw=false) {
    this.k._cancelWaiter(w.p);
    this.k[raw ? '_respondRaw' : '_respond'](w.p,result);
  }
  flock(p, fd, flags) {
    const k=this.k, o=k._ofds.get(p.fds.get(fd));
    const reply=r=>k._respond(p,r);
    if(!o)return reply({errno:'EBADF'});
    if(o.kind!=='file')return reply({errno:'EOPNOTSUPP'});
    const type=flags&~4;
    if((flags&~15)||![1,2,8].includes(type))return reply({errno:'EINVAL'});
    let s=this.locks.get(o.ino);
    if(!s){s={holders:new Map(),waiters:new Set()};this.locks.set(o.ino,s);}
    // Conversion drops the old lock before competing, as BSD flock permits.
    if(s.holders.get(o.id)===type)return reply({});
    s.holders.delete(o.id);
    this.wakeLocks(s);
    if(type===8){this.pruneLock(o.ino,s);return reply({});}
    if(this.lockFree(s,o.id,type)){this.locks.set(o.ino,s);s.holders.set(o.id,type);return reply({});}
    if(flags&4)return reply({errno:'EAGAIN'});
    this.park(p,s.waiters,{ofd:o.id,type,heldOfd:o});
  }
  lockFree(s,id,type) {
    for(const [owner,held] of s.holders)if(owner!==id&&(type===2||held===2))return false;
    return true;
  }
  wakeLocks(s) {
    for(const w of [...s.waiters])if(s.waiters.has(w)&&this.lockFree(s,w.ofd,w.type)){
      s.holders.set(w.ofd,w.type);this.finish(w,{});
    }
  }
  pruneLock(ino,s) { if(!s.holders.size&&!s.waiters.size)this.locks.delete(ino); }
  releaseOfd(o) {
    if(o.kind==='file'){
      const s=this.locks.get(o.ino);
      if(s){
        s.holders.delete(o.id);
        for(const w of [...s.waiters])if(w.ofd===o.id)this.finish(w,{errno:'EBADF'});
        this.wakeLocks(s);this.pruneLock(o.ino,s);
      }
    }else if(o.kind==='eventfd'){
      for(const w of [...o.waiters])this.finish(w,{errno:'EBADF'});
      this.k._recheckSelects();
    }
  }
  eventCreate(p,value,flags) {
    const k=this.k;
    if(flags&~(1|0x800|0x80000))return k._respond(p,{errno:'EINVAL'});
    const o=k._makeOfd('eventfd',{value:BigInt(value>>>0),semaphore:!!(flags&1),
      statusFlags:2|(flags&0x800),waiters:new Set()});
    let fd=0;while(p.fds.has(fd))fd++;
    k._ofdRef(o,p.pid);p.fds.set(fd,o.id);p.fdFlags.set(fd,flags&0x80000?1:0);
    k._respond(p,{fd});
  }
  eventReady(o) { return (o.value>0n?1:0)|(o.value<0xfffffffffffffffen?4:0); }
  eventIO(p,o,data,count) {
    const write=data!==null, k=this.k;
    if(count<8 || (write&&count!==8))return k._respond(p,{errno:'EINVAL'});
    const value=write?new DataView(data.buffer,data.byteOffset,8).getBigUint64(0,true):0n;
    if(value===0xffffffffffffffffn)return k._respond(p,{errno:'EINVAL'});
    const w={p,write,value};
    if(this.eventCan(o,w)){this.eventComplete(o,w,false);this.eventWake(o);return;}
    if(o.statusFlags&0x800)return k._respond(p,{errno:'EAGAIN'});
    this.park(p,o.waiters,{write,value,heldOfd:o});
  }
  eventCan(o,w) { return w.write?o.value+w.value<=0xfffffffffffffffen:o.value>0n; }
  eventComplete(o,w,parked) {
    let result;
    if(w.write){o.value+=w.value;result={n:8};}
    else {
      const v=o.semaphore?1n:o.value;o.value-=v;
      result=new Uint8Array(8);new DataView(result.buffer).setBigUint64(0,v,true);
    }
    if(parked)this.finish(w,result,!w.write);
    else this.k[w.write?'_respond':'_respondRaw'](w.p,result);
  }
  eventWake(o) {
    // Each completion removes a waiter; retry earlier readers after writers.
    let progress;
    do{progress=false;for(const w of [...o.waiters])if(o.waiters.has(w)&&this.eventCan(o,w)){
      this.eventComplete(o,w,true);progress=true;
    }}while(progress&&o.waiters.size);
    this.k._recheckSelects();
  }
  proc(pid) {
    let h=this.handles.get(pid);
    if(!h){h=new Map();this.handles.set(pid,h);}
    return h;
  }
  collect(s) { if(!s.linked&&!s.refs&&!s.waiters.size)this.sems.delete(s.id); }
  clone(parent,child) {
    const h=this.handles.get(parent);if(!h)return;
    this.handles.set(child,new Map(h));
    for(const [id,n] of h)this.sems.get(id).refs+=n;
  }
  release(pid) {
    const h=this.handles.get(pid);if(!h)return;
    this.handles.delete(pid);
    for(const [id,n] of h){const s=this.sems.get(id);s.refs-=n;this.collect(s);}
  }
  sem(p,r) {
    const k=this.k, reply=x=>k._respond(p,x), action=r.action;
    if(action===0||action===2){
      const name=r.name;
      if(typeof name!=='string'||name[0]!=='/'||name.length<2||name.slice(1).includes('/'))
        return reply({errno:'EINVAL'});
      if(new TextEncoder().encode(name).length>252)return reply({errno:'ENAMETOOLONG'});
      let s=this.names.get(name);
      if(action===2){
        if(!s)return reply({errno:'ENOENT'});
        this.names.delete(name);s.linked=false;this.collect(s);return reply({});
      }
      if(r.flags&~(0x40|0x80))return reply({errno:'EINVAL'});
      if(s&&(r.flags&0xc0)===0xc0)return reply({errno:'EEXIST'});
      if(!s){
        if(!(r.flags&0x40))return reply({errno:'ENOENT'});
        if(!Number.isInteger(r.value)||r.value<0||r.value>0x7fffffff)return reply({errno:'EINVAL'});
        if(this.nextSem>0x7fffffff)return reply({errno:'ENOSPC'});
        s={id:this.nextSem++,value:r.value,mode:r.mode&0o777,linked:true,refs:0,waiters:new Set()};
        this.names.set(name,s);this.sems.set(s.id,s);
      }
      // gucOS has only uid/gid 0 (setuid of other IDs fails). Like file
      // access by that privileged identity, mode bits do not deny access.
      const h=this.proc(p.pid);h.set(s.id,(h.get(s.id)||0)+1);s.refs++;
      return reply({id:s.id});
    }
    const h=this.handles.get(p.pid), s=this.sems.get(r.id);
    if(!s||!h?.has(r.id))return reply({errno:'EINVAL'});
    if(action===1){
      const n=h.get(s.id);if(n===1)h.delete(s.id);else h.set(s.id,n-1);
      s.refs--;this.collect(s);return reply({});
    }
    if(action===3){
      if(s.value===0x7fffffff)return reply({errno:'EOVERFLOW'});
      const w=s.waiters.values().next().value;
      if(w)this.finish(w,{});else s.value++;
      return reply({});
    }
    if(action===7)return reply({value:s.value});
    if(![4,5,6].includes(action))return reply({errno:'EINVAL'});
    if(s.value>0){s.value--;return reply({});}
    if(action===5)return reply({errno:'EAGAIN'});
    if(action===6&&(!Number.isFinite(r.deadline)||r.deadline<=Date.now()))
      return reply({errno:'ETIMEDOUT'});
    const w=this.park(p,s.waiters,{sem:s});
    if(action===6){
      const expire=()=>{
        if(p.waiter!==w)return;
        const left=r.deadline-Date.now();
        if(left<=0){this.finish(w,{errno:'ETIMEDOUT'});return;}
        w.timer=setTimeout(expire,Math.min(left,0x7fffffff));
      };
      expire();
    }
  }
};

// Process imports live beside the service, keeping system-process.js small.
self.installIPCImports = function(imports,ctx,client) {
  const call=(op,args,wait=false)=>{
    for(;;){
      const r=client.call(op,args,wait);
      if(wait&&r.errno==='EINTR'&&ctx.deliverSignals?.()===true)continue;
      return r;
    }
  };
  const result=r=>{if(r.errno){ctx.setErrnoName(r.errno);return -1;}return 0;};
  imports.flock=(fd,flags)=>result(call(KERNEL.OP.IPC_FLOCK,{fd,flags},true));
  imports.eventfd=(value,flags)=>{
    const r=call(KERNEL.OP.IPC_EVENTFD,{value:value>>>0,flags});
    return result(r)<0?-1:r.fd;
  };
  imports.__sem_open=(ptr,flags,mode,value)=>{
    const r=call(KERNEL.OP.IPC_SEM,{action:0,name:ctx.readString(ptr),flags,mode,value:value>>>0});
    return result(r)<0?-1:r.id;
  };
  imports.__sem_unlink=ptr=>result(call(KERNEL.OP.IPC_SEM,{action:2,name:ctx.readString(ptr)}));
  imports.__sem_op=(id,action,sec,nsec)=>{
    const r=call(KERNEL.OP.IPC_SEM,{id,action,deadline:action===6?Number(sec)*1000+nsec/1e6:undefined},action===4||action===6);
    return result(r)<0?-1:action===7?r.value:0;
  };
};
