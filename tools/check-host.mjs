// Verify deployed bytes and transport without a browser or credentials.
import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
const [origin,release,report]=process.argv.slice(2);
if(!origin||!release)throw Error('usage: node tools/check-host.mjs ORIGIN RELEASE_DIRECTORY [REPORT_JSON]');
const manifest=JSON.parse(await readFile(release+'/release.json'));
const results=[];
for(const [path,entry] of Object.entries(manifest.files)){
 const response=await fetch(new URL(path==='index.html'?'/':path,origin.endsWith('/')?origin:origin+'/'),{headers:{'User-Agent':'gucos-release-check'}});
 if(response.status!==200)throw Error(path+': HTTP '+response.status);
 const bytes=Buffer.from(await response.arrayBuffer());
 if(bytes.length!==entry.size||createHash('sha256').update(bytes).digest('hex')!==entry.sha256)throw Error('Public payload mismatch: '+path);
 for(const [name,value] of [['cross-origin-opener-policy','same-origin'],['cross-origin-embedder-policy','require-corp'],['cross-origin-resource-policy','same-origin']])if(response.headers.get(name)!==value)throw Error(path+': wrong '+name);
 const cache=response.headers.get('cache-control');if(!cache?.includes(path.startsWith('assets/')?'immutable':'no-cache'))throw Error('Wrong cache policy: '+path+' '+cache);
 results.push({path,status:response.status,sha256:entry.sha256,cache});
}
for(const path of ['/missing-asset.bin','/release.json','/pages-release.json']){
 const response=await fetch(new URL(path,origin),{headers:{'User-Agent':'gucos-release-check'}});
 if(response.status!==404)throw Error(path+': expected 404, got '+response.status);
 results.push({path,status:response.status});
}
if(report)await writeFile(report,JSON.stringify({origin,checkedAt:new Date().toISOString(),passed:true,files:results},null,2)+'\n');
console.log('PASS public hashes, isolation/cache headers and missing/private-path 404s:',origin);
