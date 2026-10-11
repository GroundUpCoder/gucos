import {DatabaseSync} from 'node:sqlite';
import {mkdir} from 'node:fs/promises';
import {join,basename,resolve} from 'node:path';
import {homedir} from 'node:os';
import {randomUUID} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {setTimeout as delay} from 'node:timers/promises';
import {processIdentity as identity,processAlive as alive,processTable} from './process-tree.mjs';
const defaultDirectory=join(homedir(),'.cache/wasm-posix/scheduler');
// Waiters poll; each poll costs one ps exec (a Gatekeeper check over SSH).
// Bound both the poll rate and the number of waiters.
const pollMs=1000,defaultMaxQueued=4;
export const lanePolicy=Object.freeze({browser:1,build:Object.freeze({general:1,small:1,'wasm-posix':1})});
export function buildLane(cwd=process.cwd()){
 // Common Git directory identifies linked worktrees, including those under /tmp.
 try{
  const common=execFileSync('git',['rev-parse','--path-format=absolute','--git-common-dir'],{cwd,encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim();
  const project=basename(resolve(common,'..'));
  return project==='small'||project==='wasm-posix'?project:'general';
 }catch{return 'general';}
}
const owner=()=>({pid:process.pid,birth:identity(process.pid)});
async function transaction(directory,change,{signal,deadline}) {
 signal?.throwIfAborted();if(Date.now()>deadline)throw Error('Scheduler deadline exceeded');
 const db=new DatabaseSync(join(directory,'scheduler.sqlite'));
 try {
  db.exec('PRAGMA busy_timeout=1000; CREATE TABLE IF NOT EXISTS state (id INTEGER PRIMARY KEY, data TEXT NOT NULL)');
  db.exec('BEGIN IMMEDIATE');
  try {
   const row=db.prepare('SELECT data FROM state WHERE id=1').get();
   const state=row?JSON.parse(row.data):{format:1,limits:{cpu:2,memoryMiB:8192},jobs:[]};
   if(state.jobs.length){const table=processTable();state.jobs=state.jobs.filter(j=>alive(j,table)||(j.children||[]).some(c=>alive(c,table)));}
   // Normalize tagged leases created by an already-running early lane client.
   // Keep their lane and lifecycle intact; only the legacy compatibility weights change.
   for(const job of state.jobs)if(job.reservation){job.cpu=state.limits.cpu;job.memoryMiB=state.limits.memoryMiB;}
   const result=change(state);
   db.prepare('INSERT OR REPLACE INTO state(id,data) VALUES(1,?)').run(JSON.stringify(state));
   db.exec('COMMIT');return result;
  }catch(error){db.exec('ROLLBACK');throw error;}
 }finally{db.close();}
}
export async function configureScheduler(limits,directory=defaultDirectory){
 if(!Number.isInteger(limits.cpu)||limits.cpu<1||!Number.isInteger(limits.memoryMiB)||limits.memoryMiB<1)throw Error('Invalid scheduler limits');
 await mkdir(directory,{recursive:true});return transaction(directory,state=>{if(state.jobs.length)throw Error('Cannot change limits while jobs are queued/running');state.limits=limits;},{deadline:Date.now()+10000});
}
export async function schedulerStatus(directory=process.env.WASM_POSIX_SCHEDULER_DIR||defaultDirectory){
 await mkdir(directory,{recursive:true});return transaction(directory,state=>({...structuredClone(state),lanePolicy}),{deadline:Date.now()+10000});
}
export async function acquireJob({cpu=1,memoryMiB=4096,exclusive=false,timeoutMs=3600000,signal,directory=process.env.WASM_POSIX_SCHEDULER_DIR||defaultDirectory,name='package',kind,lane,maxQueued=Number(process.env.WASM_POSIX_SCHEDULER_MAX_QUEUED)||defaultMaxQueued}={}){
 if(!Number.isInteger(cpu)||cpu<1||!Number.isInteger(memoryMiB)||memoryMiB<1)throw Error('Invalid job resources');
 if(kind!==undefined&&kind!=='build'&&kind!=='browser')throw Error('Invalid job kind: use build or browser');
 if(lane!==undefined&&kind!=='build')throw Error('A lane is only valid for build jobs');
 if(kind==='build'){
  const detected=buildLane();lane??=detected;
  if(!Object.hasOwn(lanePolicy.build,lane))throw Error('Invalid build lane');
  if(detected!=='general'&&lane!==detected)throw Error(`This repository belongs to the ${detected} build lane`);
 }
 const reservation=kind==='build'?`build:${lane}`:kind==='browser'?'browser':undefined;
 await mkdir(directory,{recursive:true});
 // Only descendants of the live lease owner can inherit a wrapper's reservation.
 if(process.env.MACHINE_JOB_ID){
  const state=await schedulerStatus(directory),parent=state.jobs.find(j=>j.id===process.env.MACHINE_JOB_ID&&j.status==='running'&&alive(j));
  let pid=process.pid;const table=parent?processTable():null;
  while(parent&&pid>1&&pid!==parent.pid)pid=table.get(pid)?.ppid??0;
  if(parent&&pid===parent.pid){
   if(parent.kind==='build'&&(kind!=='build'||parent.lane!==lane))throw Error('Incompatible nested reservation: reserve browser work at the outermost wrapper; do not switch build lanes');
   const inherited=async()=>{};inherited.track=async()=>{};inherited.id=parent.id;return inherited;}
 }
 const job={...owner(),id:randomUUID(),name,cpu,memoryMiB,...(kind?{kind,lane:kind==='build'?lane:undefined,reservation}:{}),status:'queued'},deadline=Date.now()+timeoutMs;
 const release=()=>transaction(directory,state=>{state.jobs=state.jobs.filter(j=>j.id!==job.id);},{deadline:Date.now()+10000});
 release.id=job.id;
 release.track=pid=>transaction(directory,state=>{const current=state.jobs.find(j=>j.id===job.id);if(current)(current.children??=[]).push({pid,birth:identity(pid)});},{deadline:Date.now()+10000});
 try{
  for(;;){
   const granted=await transaction(directory,state=>{
    // Advertise a full legacy budget so old clients cannot admit a second browser.
    // New clients use reservations, not these compatibility weights.
    if(kind){job.cpu=state.limits.cpu;job.memoryMiB=state.limits.memoryMiB;}
    if(exclusive&&!kind){cpu=state.limits.cpu;memoryMiB=state.limits.memoryMiB;job.cpu=cpu;job.memoryMiB=memoryMiB;}
    if(!kind&&(cpu>state.limits.cpu||memoryMiB>state.limits.memoryMiB))throw Error('Job exceeds configured machine budget');
    if(!state.jobs.some(j=>j.id===job.id)){
     const waiting=state.jobs.filter(j=>j.status==='queued'&&(!kind||!j.reservation||j.reservation===reservation)).length;
     if(waiting>=maxQueued)throw Object.assign(Error(`Machine job queue is full (${waiting} waiting, limit ${maxQueued}); retry after a queued job starts`),{code:'EQUEUEFULL'});
     state.jobs.push(job);
    }
    const active=state.jobs.filter(j=>j.status==='running'),first=state.jobs.find(j=>j.status==='queued');
    if(kind){
     // Old running/waiting clients retain global reservations until they drain.
     // FIFO is per lane, so a blocked Small waiter cannot stall another lane.
     const before=state.jobs.slice(0,state.jobs.findIndex(j=>j.id===job.id));
     if(active.some(j=>!j.reservation||j.reservation===reservation)||before.some(j=>j.status==='queued'&&(!j.reservation||j.reservation===reservation)))return false;
    }else if(first?.id!==job.id||active.some(j=>j.reservation)||active.reduce((n,j)=>n+j.cpu,0)+cpu>state.limits.cpu||active.reduce((n,j)=>n+j.memoryMiB,0)+memoryMiB>state.limits.memoryMiB)return false;
    state.jobs.find(j=>j.id===job.id).status='running';return true;
   },{signal,deadline});
   if(granted)return release;
   await delay(pollMs+Math.random()*250,undefined,{signal});
  }
 }catch(error){await release();throw error;}
}
