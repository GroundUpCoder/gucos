// Filesystem operations run only in the system owner. No disk handles cross workers.
function mkdirs(path){const parts=normalize(path).split('/');for(let i=2;i<parts.length;i++)systemFS.mkdir(parts.slice(0,i).join('/'),0o755);}
function writeFile(path,bytes,mode=0o644){
  path=normalize(path);mkdirs(path);if(typeof bytes==='string')bytes=encoder.encode(bytes);
  const fd=systemFS.open(path,0x40|0x200|1,mode);
  if(fd==null||fd<0)throw new Error(`open ${path}: ${systemFS._lastError}`);
  try{if(systemFS.write(fd,bytes,bytes.length)!==bytes.length)throw new Error(`write ${path}`);}finally{systemFS.close(fd);}
}
// Initial source installation is resumable until the completion marker exists.
// Do not hold the entire offline source disk in one redo transaction: the
// journal would temporarily need another disk's worth of browser storage.
function seedFiles(entries,marker,batchBytes=16<<20){
  if(systemFS.stat(marker))return false;
  let pending=0;
  for(const [path,value] of entries){
    const bytes=typeof value==='string'?encoder.encode(value):value;
    writeFile(path,bytes);pending+=bytes.byteLength;
    if(pending>=batchBytes){systemFS._s.flush();pending=0;}
  }
  writeFile(marker,'1\n');systemFS._s.flush();return true;
}
function readFile(path){
  const fd=systemFS.open(path,0,0);
  if(fd==null||fd<0)return null;
  try{const parts=[];let total=0;for(;;){const bytes=new Uint8Array(65536);const n=systemFS.read(fd,bytes,bytes.length);if(n==null||n<0)throw new Error(`read ${path}`);if(!n)break;parts.push(bytes.slice(0,n));total+=n;}const result=new Uint8Array(total);let offset=0;for(const part of parts){result.set(part,offset);offset+=part.length;}return result;}finally{systemFS.close(fd);}
}
function snapshot(dir='/', result={}){
  const h=systemFS.opendir(dir);if(h==null)return result;
  try{let entry;while((entry=systemFS.readdir(h))!==null){if(entry.name==='.'||entry.name==='..')continue;const path=normalize(dir+'/'+entry.name);const stat=systemFS.lstat(path);if(!stat)continue;if((stat.mode&0xf000)===0x4000)snapshot(path,result);else if((stat.mode&0xf000)===0x8000&&!path.startsWith('/bin/')&&!path.endsWith('.wad')){const bytes=readFile(path);if(bytes&&!bytes.includes(0)){try{result[path]=new TextDecoder('utf-8',{fatal:true}).decode(bytes);}catch{}}}}}finally{systemFS.closedir(h);}return result;
}
