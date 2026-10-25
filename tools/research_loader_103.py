from urllib.request import urlopen,Request
from pathlib import Path
import json
p=Path(__file__).resolve().parent.parent/'logs/avplayer-references-103'
for repo in ['shadps4-emu/shadPS4','LightningMods/Itemzflow']:
 try:
  info=json.load(urlopen(Request('https://api.github.com/repos/'+repo,headers={'User-Agent':'local-ps4-port'}),timeout=15))
  tree=json.load(urlopen(Request('https://api.github.com/repos/'+repo+'/git/trees/'+info['default_branch']+'?recursive=1',headers={'User-Agent':'local-ps4-port'}),timeout=20))
  paths=[v['path'] for v in tree.get('tree',[]) if ('kernel' in v['path'] and v['path'].endswith('.cpp')) or ('video' in v['path'].lower() and v['path'].endswith(('.cpp','.c','.h')))]
  print(repo,json.dumps(paths[:80]))
  if repo.startswith('shad'):
   for path in paths:
    if any(x in path for x in ['libkernel.cpp','module','modules.cpp']):
     data=urlopen('https://raw.githubusercontent.com/'+repo+'/'+info['default_branch']+'/'+path,timeout=15).read();(p/path.replace('/','_')).write_bytes(data)
 except Exception as e:print(str(e))
