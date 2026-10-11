import json,collections,re,gzip,base64,pathlib
root=pathlib.Path(__file__).parent
trace=json.loads(gzip.decompress((root/'trace.json.gz').read_bytes()))['traceEvents']
labels={}
html=pathlib.Path('dist/index.html').read_text()
for part in re.search(r'<script id="bundle"[^>]*>(.*?)</script>',html,re.S)[1].split():
 payload=json.loads(gzip.decompress(base64.b64decode(part)))
 for k,v in payload.get('prebuilt',{}).items():
  if 'url' in v:labels[v['url'].split('/')[-1]]=k
 for k,v in payload.items():
  if k.endswith('URL') and isinstance(v,str):labels[v.split('/')[-1]]=k
requests={}
for e in trace:
 d=e.get('args',{}).get('data',{});key=(e['pid'],d.get('requestId'))
 if e['name']=='ResourceSendRequest' and d.get('url','').startswith('https://os.groundupcoder.net'):
  requests[key]={'url':d['url'],'start':e['ts']/1000,'type':d.get('resourceType')}
 elif key in requests and e['name']=='ResourceReceiveResponse':requests[key].update(response=e['ts']/1000,cached=d.get('fromCache'),status=d.get('statusCode'))
 elif key in requests and e['name']=='ResourceFinish':requests[key].update(end=e['ts']/1000,bytes=d.get('encodedDataLength'))
nav=sorted(r['start'] for r in requests.values() if r['type']=='Document')
runs=[]
for i,start in enumerate(nav):
 rs=[]
 for r in requests.values():
  if start<=r['start']<(nav[i+1] if i+1<len(nav) else float('inf')):
   rs.append({**r,'label':labels.get(r['url'].split('/')[-1],r['url']),'start':round(r['start']-start,1),'response':round(r.get('response',r['start'])-start,1),'end':round(r.get('end',r['start'])-start,1)})
 runs.append(sorted(rs,key=lambda x:x['start']))
profiles={}
for e in trace:
 if e['name']!='ProfileChunk':continue
 key=(e['pid'],e.get('id'));p=profiles.setdefault(key,{'nodes':{},'samples':[]});d=e['args']['data'];cpu=d.get('cpuProfile',{})
 for n in cpu.get('nodes',[]):p['nodes'][n['id']]=n
 p['samples'].extend(zip(cpu.get('samples',[]),d.get('timeDeltas',[])))
cpu=[]
for key,p in profiles.items():
 if not any('os.groundupcoder.net' in n['callFrame'].get('url','') for n in p['nodes'].values()):continue
 totals=collections.Counter()
 for sid,dt in p['samples']:
  n=p['nodes'].get(sid,{}).get('callFrame',{});totals[(n.get('functionName','?'),n.get('url',''))]+=max(0,dt)/1000
 cpu.append({'pid':key[0],'top':[{'function':k[0],'url':k[1],'sampleMs':round(v,1)} for k,v in totals.most_common(20)]})
(root/'analysis.json').write_text(json.dumps({'networkRuns':runs,'cpuProfiles':cpu},indent=2))
for i,rs in enumerate(runs):
 print('RUN',i,'bytes',sum(r.get('bytes') or 0 for r in rs))
 for r in rs:print(r['label'],r['start'],r['response'],r['end'],r.get('bytes'),r.get('cached'))
print('CPU',json.dumps(cpu,indent=2))
