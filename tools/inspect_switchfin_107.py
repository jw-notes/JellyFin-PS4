import json,urllib.request
from pathlib import Path
p=Path('logs/switchfin-reference-107');p.mkdir(exist_ok=True)
repo='dragonflylee/switchfin'; info=json.load(urllib.request.urlopen('https://api.github.com/repos/'+repo,timeout=20));branch=info['default_branch']
tree=json.load(urllib.request.urlopen('https://api.github.com/repos/'+repo+'/git/trees/'+branch+'?recursive=1',timeout=30))
(p/'tree.json').write_text(json.dumps(tree))
paths=[v['path'] for v in tree['tree'] if v['type']=='blob' and any(s in v['path'].lower() for s in ['mpv','player','playback','ps4','cmakelists','readme']) and v['path'].endswith(('.cpp','.hpp','.h','.c','.md','.txt','.cmake'))]
print('Branch:',branch,'Relevant paths:',*paths,sep='\n')
for path in ['README.md','CMakeLists.txt','LICENSE']+paths[:25]:
 try:
  content=urllib.request.urlopen('https://raw.githubusercontent.com/'+repo+'/'+branch+'/'+path,timeout=20).read();(p/path.replace('/','_')).write_bytes(content)
 except Exception as e: print(path,type(e).__name__)
