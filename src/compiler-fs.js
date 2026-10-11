// A synchronous compiler can wait here; the filesystem owner never waits.
// One outstanding request per worker. Large replies are drained in chunks,
// keeping the shared mailbox bounded even for archives and source snapshots.
self.CompilerFS = {
  client(send) {
    const control=new Int32Array(new SharedArrayBuffer(16));
    const data=new Uint8Array(new SharedArrayBuffer(1024*1024));
    const decoder=new TextDecoder();
    function exchange(method,args) {
      Atomics.store(control,0,0);
      send({type:'compiler-fs',method,args,control:control.buffer,data:data.buffer});
      while(Atomics.load(control,0)===0)Atomics.wait(control,0,0);
    }
    return (method,...args)=>{
      exchange(method,args);
      const status=Atomics.load(control,0),length=control[1],binary=control[2];
      const result=new Uint8Array(length);
      for(let offset=0;offset<length;offset+=data.length){
        if(offset)exchange('continue',[offset]);
        result.set(data.subarray(0,Math.min(data.length,length-offset)),offset);
      }
      if(status===2)throw new Error(decoder.decode(result));
      return binary?result:JSON.parse(decoder.decode(result));
    };
  },
  server(operations) {
    const encoder=new TextEncoder();let pending=null;
    return message=>{
      const control=new Int32Array(message.control),data=new Uint8Array(message.data);
      let offset=0,status=1,binary=0;
      try{
        if(message.method==='continue'){
          offset=message.args[0];
          if(!pending||!Number.isSafeInteger(offset)||offset<0||offset>=pending.bytes.length)throw new Error('Invalid compiler filesystem continuation');
        }else{
          if(!Object.hasOwn(operations,message.method))throw new Error('Unknown compiler filesystem operation: '+message.method);
          const value=operations[message.method](...message.args);
          binary=value instanceof Uint8Array?1:0;
          pending={bytes:binary?value:encoder.encode(JSON.stringify(value??null)),binary};
        }
      }catch(error){status=2;pending={bytes:encoder.encode(error.message),binary:0};offset=0;}
      control[1]=pending.bytes.length;control[2]=pending.binary;
      data.set(pending.bytes.subarray(offset,offset+data.length));
      if(offset+data.length>=pending.bytes.length)pending=null;
      Atomics.store(control,0,status);Atomics.notify(control,0);
    };
  }
};
