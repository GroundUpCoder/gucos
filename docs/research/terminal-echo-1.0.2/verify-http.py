import concurrent.futures,hashlib,json,pathlib,sys,urllib.request,urllib.error
base,out=sys.argv[1:];manifest=json.loads(pathlib.Path('releases/terminal-echo-1.0.2-final/pages-release.json').read_text())
headers={'User-Agent':'Mozilla/5.0 (gucOS release verification)','Accept':'*/*'}
def check(item):
 name,want=item
 if name=='_headers':return None
 req=urllib.request.Request(base+('/' if name=='index.html' else '/'+name.removesuffix('.html')),headers=headers)
 with urllib.request.urlopen(req,timeout=60) as r:
  data=r.read();assert len(data)==want['size'],name;assert hashlib.sha256(data).hexdigest()==want['sha256'],name
  for k,v in [('Cross-Origin-Opener-Policy','same-origin'),('Cross-Origin-Embedder-Policy','require-corp'),('Cross-Origin-Resource-Policy','same-origin')]:assert r.headers.get(k)==v,(name,k,r.headers.get(k))
  cache=r.headers.get('Cache-Control','');assert ('immutable' in cache if name.startswith('assets/') else 'no-cache' in cache),(name,cache)
 return name
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:files=[x for x in pool.map(check,manifest['files'].items()) if x]
for name in ['absent-release-probe-cc3227','release.json','pages-release.json']:
 try:
  urllib.request.urlopen(urllib.request.Request(base+'/'+name,headers=headers),timeout=30)
  raise AssertionError(name+' should be 404')
 except urllib.error.HTTPError as e:assert e.code==404,(name,e.code)
record={'status':'passed','url':base,'files':len(files),'runtime':'cc3227370c2ccfb2a5ed7ea97627dbd512ec19abfacd7bf2f5ca8dc27421f309','checks':['exact sizes and SHA256','COOP/COEP/CORP','cache policy','missing objects and operator manifests return 404']}
pathlib.Path(out).write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
