from pathlib import Path
from urllib.request import urlopen,Request
import json,concurrent.futures
root=Path(__file__).resolve().parent.parent/'logs/avplayer-references-103';root.mkdir(exist_ok=True)
repos=['LightningMods/PS4-Store','LightningMods/Itemzflow','orbisdev/liborbis']
def inspect(repo):
 try:
  info=json.load(urlopen(Request('https://api.github.com/repos/'+repo,headers={'User-Agent':'local-ps4-port'}),timeout=15))
  tree=json.load(urlopen(Request('https://api.github.com/repos/'+repo+'/git/trees/'+info['default_branch']+'?recursive=1',headers={'User-Agent':'local-ps4-port'}),timeout=20))
  paths=[v['path'] for v in tree.get('tree',[]) if any(s in v['path'].lower() for s in ['avplayer','video_player','videoplayer'])]
  result={'repo':repo,'branch':info['default_branch'],'paths':paths}
  for path in paths:
   if path.endswith(('.cpp','.c','.h')):
    data=urlopen('https://raw.githubusercontent.com/'+repo+'/'+info['default_branch']+'/'+path,timeout=15).read()
    (root/(repo.replace('/','-')+'-'+path.replace('/','_'))).write_bytes(data)
  return result
 except Exception as e:return {'repo':repo,'error':str(e)}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:results=list(pool.map(inspect,repos))
(root/'sources.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
u='https://raw.githubusercontent.com/shadps4-emu/shadPS4/main/src/core/libraries/kernel/module.cpp'
try:(root/'shad-module.cpp').write_bytes(urlopen(u,timeout=15).read())
except Exception as e:print(str(e))
