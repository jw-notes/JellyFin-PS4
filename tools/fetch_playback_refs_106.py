import urllib.request,json
from pathlib import Path
out=Path('logs/playback-references-106');out.mkdir(exist_ok=True)
for repo in ['LightningMods/Itemzflow','LightningMods/PS4-Store']:
 try:
  info=json.load(urllib.request.urlopen('https://api.github.com/repos/'+repo,timeout=15));branch=info['default_branch']
  tree=json.load(urllib.request.urlopen('https://api.github.com/repos/'+repo+'/git/trees/'+branch+'?recursive=1',timeout=25))
  matches=[v['path'] for v in tree['tree'] if any(s in v['path'].lower() for s in ['avplayer','video','player']) and v['path'].endswith(('.cpp','.c','.h'))]
  print(repo,matches[:30]);(out/(repo.split('/')[-1]+'-files.json')).write_text(json.dumps(matches))
  for path in matches[:15]:
   data=urllib.request.urlopen('https://raw.githubusercontent.com/'+repo+'/'+branch+'/'+path,timeout=15).read();(out/(repo.split('/')[-1]+'-'+path.replace('/','_'))).write_bytes(data)
 except Exception as e: print(repo,type(e).__name__,str(e))
