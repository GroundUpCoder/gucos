#!/usr/bin/env node
// Machine-wide entry point; shares the queue used by existing package worktrees.
import {spawn} from 'node:child_process';
import {setTimeout as delay} from 'node:timers/promises';
import {acquireJob,schedulerStatus} from './job-scheduler.mjs';
import {descendants,signalProcesses} from './process-tree.mjs';

const args=process.argv.slice(2);
if(args[0]==='status'){
 console.log(JSON.stringify(await schedulerStatus(),null,2));
}else if(args[0]==='run'){
 args.shift();let timeoutSeconds=1800,kind='browser',lane;
 while(args[0]&&args[0]!=='--'){
  const flag=args.shift(),value=args.shift();
  if(flag==='--timeout')timeoutSeconds=Number(value);
  else if(flag==='--kind')kind=value;
  else if(flag==='--lane')lane=value;
  else throw Error(`Unknown option: ${flag}`);
 }
 if(args.shift()!=='--'||!args.length||!Number.isFinite(timeoutSeconds)||timeoutSeconds<=0)throw Error('Usage: machine-job run [--kind build|browser] [--lane general|small|wasm-posix] [--timeout SECONDS] -- COMMAND [ARG...]');
 const abort=new AbortController();let child,interrupted,timer,release;
 const interrupt=signal=>{interrupted=signal;abort.abort();if(child?.pid)killGroup('SIGTERM');};
 const onInt=()=>interrupt('SIGINT'),onTerm=()=>interrupt('SIGTERM');
 function killGroup(signal){try{process.kill(-child.pid,signal);}catch(error){if(error.code!=='ESRCH')throw error;}}
 process.on('SIGINT',onInt);process.on('SIGTERM',onTerm);
 try{
  console.error(`[machine-job] queued: ${args.join(' ')}`);
  release=await acquireJob({name:`${process.cwd()}: ${args.join(' ')}`,kind,lane,signal:abort.signal});
  abort.signal.throwIfAborted();
  console.error(`[machine-job] running: ${release.id}`);
  child=spawn(args[0],args.slice(1),{stdio:'inherit',detached:true,env:{...process.env,MACHINE_JOB_ID:release.id,WASM_POSIX_SCHEDULER_HELD:'1'}});
  const exited=new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',(code,signal)=>resolve({code,signal}));});
  // Attach rejection handling before the asynchronous tracking operation.
  exited.catch(()=>{});
  if(child.pid)await release.track(child.pid);
  timer=setTimeout(()=>{interrupted='timeout';killGroup('SIGTERM');},timeoutSeconds*1000);
  const force=setInterval(()=>{if(interrupted&&child?.pid)killGroup('SIGKILL');},1000);
  try{const result=await exited;process.exitCode=interrupted==='timeout'?124:interrupted==='SIGINT'?130:interrupted?143:result.code??1;}
  finally{clearInterval(force);}
 }catch(error){
  if(interrupted)process.exitCode=interrupted==='SIGINT'?130:143;
  else if(error.code==='EQUEUEFULL'){console.error(`[machine-job] ${error.message}`);process.exitCode=75;}
  else{console.error(`[machine-job] ${error.stack||error}`);process.exitCode=1;}
 }finally{
  clearTimeout(timer);
  if(child?.pid){
   let remaining=[];try{remaining=descendants(process.pid);}catch(error){console.error(`[machine-job] descendant snapshot failed: ${error.message}`);}
   const signalRemaining=signal=>{try{signalProcesses(remaining,signal);}catch(error){console.error(`[machine-job] ${signal} snapshot failed: ${error.message}`);}};
   killGroup('SIGTERM');signalRemaining('SIGTERM');
   await delay(200);
   killGroup('SIGKILL');signalRemaining('SIGKILL');
  }
  if(release)await release();
  process.off('SIGINT',onInt);process.off('SIGTERM',onTerm);
 }
}else{
 console.error('Usage: machine-job status | machine-job run [--kind build|browser] [--lane general|small|wasm-posix] [--timeout SECONDS] -- COMMAND [ARG...]');
 process.exitCode=2;
}
