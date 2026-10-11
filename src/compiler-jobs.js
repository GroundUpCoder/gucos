// Compiler lifetime and cancellation belong to the owner; compiler CPU work
// belongs to disposable workers. The supplied operations retain disk ownership.
class CompilerJobs {
  constructor(url,operations,{buildId='',toolchain=null,output=()=>{},onState=()=>{},maxJobs=2}={}) {
    Object.assign(this,{url,operations,buildId,toolchain,output,onState});
    this.jobs=new Map();this.nextId=1;this.queue=[];this.active=0;this.closed=false;
    this.maxJobs=Math.max(1,Math.min(2,Math.floor(maxJobs)||1));
  }
  run(request,{signal,pid=null}={}) {
    if(this.closed||signal?.aborted)return Promise.reject(new Error('Compilation cancelled'));
    return new Promise((resolve,reject)=>{
      const id=this.nextId++;let worker=null;
      const serve=CompilerFS.server(this.operations);
      let settled=false;
      const finish=(error,result)=>{
        if(settled)return;settled=true;
        if(worker){worker.terminate();this.active--;}
        this.queue=this.queue.filter(job=>job.id!==id);signal?.removeEventListener('abort',cancel);this.jobs.delete(id);
        if(worker)this.onState({id,pid,active:false});
        this.drain();
        if(error)reject(error);else resolve(result);
      };
      const cancel=()=>finish(new Error('Compilation cancelled'));
      this.jobs.set(id,cancel);signal?.addEventListener('abort',cancel,{once:true});
      const start=()=>{
        try{worker=new Worker(this.url,{name:`compiler-${pid??'boot'}-${id}`});}catch(error){finish(error);return;}
        this.active++;
        worker.onmessage=({data})=>{
          if(settled)return;
          if(data.type==='compiler-fs')serve(data);
          else if(data.type==='output')this.output(data.text);
          else if(data.type==='done')finish(null,data.result);
          else if(data.type==='error')finish(new Error(data.text));
        };
        worker.onerror=event=>{event.preventDefault();finish(new Error(event.message));};
        this.onState({id,pid,active:true});
        try{worker.postMessage({request,buildId:this.buildId,toolchain:this.toolchain});}catch(error){finish(error);}
      };
      this.queue.push({id,start});this.drain();
    });
  }
  drain(){while(!this.closed&&this.active<this.maxJobs&&this.queue.length)this.queue.shift().start();}
  close(){this.closed=true;for(const cancel of [...this.jobs.values()])cancel();}
}
