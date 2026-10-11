// Deliberately strict unified text patches: no fuzz, renames or binary patches.
export function safePath(p){return typeof p==='string'&&/^[A-Za-z0-9_+.@/-]+$/.test(p)&&!p.startsWith('/')&&!p.split('/').some(x=>!x||x==='.'||x==='..');}
export function applyPatch(files,text){
 const lines=text.split('\n');if(lines.pop()!=='')throw Error('Patch must end with newline');
 let i=0,changes=0;
 while(i<lines.length){
  if(!lines[i].startsWith('--- a/'))throw Error('Expected --- a/PATH');
  const path=lines[i++].slice(6).split('\t')[0];
  if(!safePath(path)||lines[i++]!=='+++ b/'+path||!files.has(path))throw Error('Unsupported patch path: '+path);
  const source=files.get(path).toString('utf8').split('\n');if(source.pop()!=='')throw Error('Patched input must end with newline');
  const result=[];let cursor=0,hunks=0;
  while(i<lines.length&&lines[i].startsWith('@@ ')){
   const m=/^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@.*$/.exec(lines[i++]);if(!m)throw Error('Invalid patch hunk');
   const start=Number(m[1])-1,oldCount=Number(m[2]??1),newCount=Number(m[4]??1);
   if(start<cursor||start>source.length)throw Error('Patch hunk order');
   result.push(...source.slice(cursor,start));cursor=start;
   if(result.length!==Number(m[3])-1)throw Error('Patch destination line differs');
   let old=0,added=0;
   while(i<lines.length&&!lines[i].startsWith('@@ ')&&!lines[i].startsWith('--- a/')){
    const line=lines[i++],op=line[0],value=line.slice(1);
    if(![' ','-','+'].includes(op))throw Error('Unsupported patch line');
    if(op!=='+'){if(source[cursor++]!==value)throw Error('Patch context mismatch: '+path);old++;}
    if(op!=='-'){result.push(value);added++;}
   }
   if(old!==oldCount||added!==newCount)throw Error('Patch line counts differ');hunks++;
  }
  if(!hunks)throw Error('Empty file patch');
  result.push(...source.slice(cursor));files.set(path,Buffer.from(result.join('\n')+'\n'));changes++;
 }
 if(!changes)throw Error('Empty patch');
}
