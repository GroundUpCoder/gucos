// The compiler sees synchronous file operations, serviced by the owner while
// this worker sleeps. It never opens OPFS or receives the kernel's disk state.
const compilerRPC=CompilerFS.client(message=>postMessage(message));
const decoder=new TextDecoder();
const readFile=path=>compilerRPC('readFile',path);
const writeFile=(path,bytes,mode=0o644)=>compilerRPC('writeFile',path,bytes,mode);
const snapshot=dir=>compilerRPC('snapshot',dir);
const systemFS={stat:path=>compilerRPC('stat',path),chmod:(path,mode)=>compilerRPC('chmod',path,mode),_s:{flush:()=>compilerRPC('flush')}};
self.onmessage=({data})=>{
  try{
    buildId=data.buildId;
    compilerToolchain=data.toolchain;
    const r=data.request,output=text=>postMessage({type:'output',text});
    let result;
    if(r.kind==='command')result=compileCommand(r.argv,r.cwd);
    else if(r.kind==='project'){
      const built=buildProject(r.sources,r.manifest,r.out,output,()=>output(r.notice));
      result=built?{stats:built.stats}:null;
    }else if(r.kind==='program'){
      const built=compile(r.sources,r.inputs,[],output);
      writeFile(r.out,built.bytes,0o755);result={stats:built.stats};
    }else throw new Error('Unknown compiler job');
    postMessage({type:'done',result});
  }catch(error){postMessage({type:'error',text:error.stack||String(error)});}
};
