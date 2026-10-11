import json
from pathlib import Path
root=Path(__file__).resolve().parent
for mode in ['candidate','live']:
 path=root/mode/'result.json'
 if not path.exists():continue
 r=json.loads(path.read_text());m=r.get('measurement')
 if not m:continue
 events=m['events']
 def time(prefix):
  return next(x['ms'] for x in events if x['text'].startswith(prefix))
 packages=[]
 for name in ['x11-server','x11-toolkit','x11-widgets','jwm']:
  download=time('gucman: Downloading '+name+' archive')
  verify=time('gucman: Verifying download: '+name+' archive')
  unpack=time('gucman: Unpacking: '+name)
  copy=time('gucman: Installing '+name+': 0/')
  checksums=time('gucman: Preparing checksums: '+name)
  save=time('gucman: Saving transaction: '+name)
  packages.append(dict(name=name,downloadSeconds=(verify-download)/1000,archiveVerificationSeconds=(unpack-verify)/1000,unpackSeconds=(copy-unpack)/1000,fileCopySeconds=(checksums-copy)/1000,checksumPreparationSeconds=(save-checksums)/1000))
 result=dict(firstVisibleMs=m['first'],totalSeconds=m['done']/1000,packages=packages)
 (root/mode/'timings.json').write_text(json.dumps(result,indent=2)+'\n')
 print(mode,json.dumps(result))
