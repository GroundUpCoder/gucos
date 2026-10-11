from pathlib import Path
import subprocess,json,hashlib,datetime
e=Path(__file__).parent
snap=Path('/home/ubuntu/git/gucos-core-snapshots/ipc-1.0.2-be3a9ce7')
def hashes():return {str(p.relative_to(snap)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(snap.rglob('*')) if p.is_file()}
before=hashes();(e/'before-hashes.json').write_text(json.dumps(before,indent=2)+'\n')
results=[]
for name,script in [('ipc-sockets', 'ipc-sockets.mjs'), ('ipc-services', 'ipc-services.mjs'), ('ipc-terminal', 'ipc-terminal.mjs'), ('ipc-symbols', 'ipc-symbols.mjs'), ('closefrom', 'closefrom.mjs'), ('platform-posix', 'platform-posix.mjs'), ('retained-abi', 'retained-abi/runner.mjs'), ('published-jsmn', 'published-jsmn/runner.mjs')]:
 print('START '+name,flush=True);start=datetime.datetime.now(datetime.timezone.utc).isoformat()
 with (e/(name+'.log')).open('w') as log:code=subprocess.call(['node',str(e/script)],stdout=log,stderr=subprocess.STDOUT,cwd='/home/ubuntu/git/gucos-ipc-final')
 print((e/(name+'.log')).read_text(),flush=True)
 results.append({'test':name,'exitCode':code,'startedUTC':start,'completedUTC':datetime.datetime.now(datetime.timezone.utc).isoformat()})
 (e/'results.json').write_text(json.dumps(results,indent=2)+'\n')
after=hashes();(e/'after-hashes.json').write_text(json.dumps(after,indent=2)+'\n')
print('SNAPSHOT-UNCHANGED '+str(before==after),flush=True)
raise SystemExit(int(any(x['exitCode'] for x in results) or before!=after))
