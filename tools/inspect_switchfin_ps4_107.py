from pathlib import Path
import json,urllib.request
p=Path('logs/switchfin-reference-107');tree=json.loads((p/'tree.json').read_text())
paths=[v['path'] for v in tree['tree'] if v['type']=='blob' and (v['path'].startswith('scripts/ps4/') or v['path']=='.gitmodules' or v['path'].startswith('.github/workflows/'))]
print(*paths,sep='\n')
for path in paths:
 try:(p/path.replace('/','_')).write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/dragonflylee/switchfin/dev/'+path,timeout=20).read())
 except Exception as e:print(path,type(e).__name__)
