// Upload a reviewed prebuilt release; never builds or reads package sources.
import {readFile,readdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
const [release,...flags]=process.argv.slice(2);
if(!release||flags.some(x=>x!=='--preview'))throw Error('usage: node tools/deploy-pages.mjs RELEASE_DIRECTORY [--preview]');
if(!process.env.CLOUDFLARE_ACCOUNT_ID||!process.env.CLOUDFLARE_API_TOKEN)throw Error('CLOUDFLARE_ACCOUNT_ID and CLOUDFLARE_API_TOKEN are required');
const root=resolve(release),site=join(root,'site'),manifest=JSON.parse(await readFile(join(root,'pages-release.json')));
for(const [path,entry] of Object.entries(manifest.files)){
 const b=await readFile(join(site,path));
 if(b.length!==entry.size||createHash('sha256').update(b).digest('hex')!==entry.sha256)throw Error('Release mismatch: '+path);
}
const expected=new Set(Object.keys(manifest.files));
async function check(dir,prefix=''){for(const e of await readdir(dir,{withFileTypes:true})){
 const p=prefix+e.name;if(e.isDirectory())await check(join(dir,e.name),p+'/');
 else if(!e.isFile()||!expected.delete(p))throw Error('Unexpected upload entry: '+p);
}}
await check(site);if(expected.size)throw Error('Missing upload entries: '+[...expected]);
const branch=flags.includes('--preview')?'migration-preview':'main';
const result=spawnSync('pnpm',['exec','wrangler','pages','deploy',site,'--project-name',process.env.CLOUDFLARE_PAGES_PROJECT||'gucos','--branch',branch,'--commit-dirty=true'],{stdio:'inherit'});
if(result.error)throw result.error;
process.exit(result.status??1);
