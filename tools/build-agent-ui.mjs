import {build} from 'esbuild';
import {readFile,mkdir,readdir} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {dirname,join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
export async function buildAgentUI(){
 await mkdir(new URL('../.cache/agent-ui/',import.meta.url),{recursive:true});
 execFileSync('pnpm',['exec','tailwindcss','-i','apps/agent-ui/cc/index.css','-o','.cache/agent-ui/style.css','--minify'],{cwd:root,stdio:'inherit'});
 const result=await build({absWorkingDir:root,entryPoints:['apps/agent-ui/main.tsx'],bundle:true,metafile:true,write:false,format:'iife',jsx:'automatic',minify:true,target:['es2022'],define:{'process.env.NODE_ENV':'"production"'},legalComments:'inline'});
 const js=result.outputFiles[0].text.replaceAll('</script','<\\/script');
 const css=await readFile(new URL('../.cache/agent-ui/style.css',import.meta.url),'utf8');
 const licenses=new Map(),missing=[];
 for(const input of [...Object.keys(result.metafile.inputs),'node_modules/tailwindcss/package.json','node_modules/@tailwindcss/typography/package.json']){
  if(!input.includes('node_modules'))continue;
  let dir=dirname(resolve(root,input));
  while(dir!==dirname(dir)){
   try{const pkg=JSON.parse(await readFile(join(dir,'package.json'),'utf8'));if(pkg.name&&pkg.version){const id=pkg.name+'@'+pkg.version;if(!licenses.has(id)){const texts=[];for(const name of await readdir(dir))if(/^(licen[sc]e|copying|notice)(\.|$)/i.test(name))texts.push(await readFile(join(dir,name),'utf8'));if(!texts.length)texts.push(await readFile(join(root,'vendor/agent-ui-licenses',pkg.name.replaceAll('/','_')+'.txt'),'utf8').catch(()=>{missing.push(id);return '';}));licenses.set(id,texts.join('\n'));}break;}}catch(e){if(e.code!=='ENOENT')throw e;}
   dir=dirname(dir);
  }
 }
 if(missing.length)throw Error('Missing license notices: '+missing.join(', '));
 const notices=[...licenses].sort(([a],[b])=>a.localeCompare(b)).map(([name,text])=>name+'\n'+text).join('\n\n');
 return {notices,html:'<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>gucOS Agent</title><style>'+css+'</style><div id="root"></div><script>'+js+'</script></html>'};
}
