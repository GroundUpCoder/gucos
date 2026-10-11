// Bootstrap directory listing. The BusyBox session uses the real ls applet.
function listDirectory(args, fileNames, cwd='/', directories=SYSTEM_DIRECTORIES) {
  let all=false, classify=false, directory=false, recursive=false;
  const operands=[]; let options=true;
  for(const arg of args) {
    if(options&&arg==='--'){options=false;continue;}
    if(options&&arg.startsWith('-')&&arg!=='-') {
      for(const flag of arg.slice(1)) {
        if(flag==='a')all=true;
        else if(flag==='F')classify=true;
        else if(flag==='d')directory=true;
        else if(flag==='R')recursive=true;
        else if(flag!=='1')throw new Error(`ls: unsupported option -${flag}; use sh for full BusyBox ls`);
      }
    } else operands.push(arg);
  }
  if(!operands.length)operands.push('.');
  const names=new Set(fileNames), dirs=new Set(directories);
  for(const name of names) {
    const parts=name.split('/');
    for(let i=2;i<parts.length;i++)dirs.add(parts.slice(0,i).join('/'));
  }
  const resolve=p=>normalize(p.startsWith('/')?p:cwd+'/'+p);
  const children=path=>{
    const prefix=path==='/'?'/':path+'/';
    return [...new Set([...names,...dirs].filter(n=>n.startsWith(prefix)&&n!==path).map(n=>n.slice(prefix.length).split('/')[0]))].filter(n=>n&&(all||!n.startsWith('.'))).sort();
  };
  const output=[];
  function visit(path, label, header) {
    if(header){if(output.length)output.push('');output.push(label+':');}
    const entries=children(path);
    if(all)output.push('.'+(classify?'/':''),'..'+(classify?'/':''));
    for(const name of entries)output.push(name+(classify&&dirs.has(resolve(path+'/'+name))?'/':''));
    if(recursive)for(const name of entries){const child=resolve(path+'/'+name);if(dirs.has(child))visit(child,child,true);}
  }
  for(const operand of operands){
    const path=resolve(operand);
    if(!names.has(path)&&!dirs.has(path))throw new Error(`ls: ${operand}: No such file or directory`);
    if(directory||!dirs.has(path))output.push(operand+(classify&&dirs.has(path)&&!operand.endsWith('/')?'/':''));
    else visit(path,operand,operands.length>1||recursive);
  }
  return output.length?output.join('\n')+'\n':'';
}
