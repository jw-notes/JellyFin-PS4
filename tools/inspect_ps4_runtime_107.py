from pathlib import Path
import urllib.request,json
p=Path('logs/switchfin-reference-107')
tree=json.load(urllib.request.urlopen('https://api.github.com/repos/dragonflylee/borealis/git/trees/wiliwili?recursive=1',timeout=20))
paths=[v['path'] for v in tree['tree'] if v['type']=='blob' and ('ps4/sce_module' in v['path'] or 'ps4' in v['path'] and v['path'].endswith(('.cpp','.c','.cmake')))]
print(*paths,sep='\n')
for path in paths:
 if 'sce_module' in path:continue
 if any(s in path for s in ['video','platform','toolchain','ps4.cmake']):
  try:(p/('borealis-'+path.replace('/','_'))).write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/dragonflylee/borealis/wiliwili/'+path,timeout=15).read())
  except Exception:pass
