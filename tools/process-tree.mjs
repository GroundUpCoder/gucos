import {execFileSync} from 'node:child_process';
// One ps exec per snapshot. Each exec from an SSH session costs a Gatekeeper
// (syspolicyd) check, so never run ps once per process in a loop.
// A failed snapshot throws: an empty table would make live leases look dead.
export function processTable(){
 const table=new Map(),text=execFileSync('ps',['-axo','pid=,ppid=,lstart='],{encoding:'utf8'});
 for(const line of text.split('\n')){const m=line.match(/^\s*(\d+)\s+(\d+)\s+(.*\S)\s*$/);if(m)table.set(Number(m[1]),{ppid:Number(m[2]),birth:m[3]});}
 return table;
}
export function processIdentity(pid,table){
 if(table)return table.get(Number(pid))?.birth??'';
 try{return execFileSync('ps',['-p',String(pid),'-o','lstart='],{encoding:'utf8'}).trim();}catch{return '';}
}
export function processAlive(record,table){try{process.kill(record.pid,0);return processIdentity(record.pid,table)===record.birth;}catch{return false;}}
export function descendants(pid,table=processTable()){
 const found=[],visit=parent=>{for(const [child,row] of table)if(row.ppid===parent){visit(child);found.push({pid:child,birth:row.birth});}};visit(pid);return found;
}
export function signalProcesses(records,signal){
 if(!records.length)return;const table=processTable();
 for(const r of records)if(processAlive(r,table))try{process.kill(r.pid,signal);}catch(e){if(e.code!=='ESRCH')throw e;}
}
