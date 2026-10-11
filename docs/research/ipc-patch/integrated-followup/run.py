import pathlib,subprocess,json,hashlib,datetime,os
r=pathlib.Path('/home/ubuntu/git/gucos-ipc-integration')
e=pathlib.Path(__file__).parent
os.chdir(r)
def capture(label):
 state={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'status':subprocess.check_output(['git','status','--short'],text=True),'dist':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((r/'dist').rglob('*')) if p.is_file()}}
 (e/(label+'.json')).write_text(json.dumps(state,indent=2)+'\n')
 (e/(label+'-build.json')).write_bytes((r/'dist/build.json').read_bytes())
 return state
pre=capture('before')
assert json.loads((r/'dist/build.json').read_text())['runtime']=='baae3b7591a51a357d4fba3fc8e96135790025d61a6c98453e28100ea99f1aad'
results=[]
for name,path in [('ipc-symbols','tests/ipc-symbols.mjs'),('ipc-adapted',str(e/'run-probes.mjs')),('platform-posix','tests/platform-posix.mjs')]:
 start=datetime.datetime.now(datetime.timezone.utc).isoformat()
 print('START',name,start,flush=True)
 with (e/(name+'.log')).open('w') as log: code=subprocess.call(['node',path],stdout=log,stderr=subprocess.STDOUT)
 results.append(dict(test=name,command=['node',path],exitCode=code,startUTC=start,endUTC=datetime.datetime.now(datetime.timezone.utc).isoformat()))
 print((e/(name+'.log')).read_text(),flush=True)
 print('END',name,'exit',code,flush=True)
 (e/'results.json').write_text(json.dumps(results,indent=2)+'\n')
post=capture('after')
comparison={k:pre[k]==post[k] for k in ['head','status','dist']}
(e/'unchanged.json').write_text(json.dumps(comparison,indent=2)+'\n')
print('UNCHANGED',json.dumps(comparison),flush=True)
raise SystemExit(int(any(v['exitCode'] for v in results) or not comparison['dist']))
