from ftplib import FTP
from pathlib import Path
import json
p=Path(__file__).resolve().parent.parent/'logs'
f=FTP();f.connect('192.168.1.26',2121,timeout=15);f.login()
result={}
for path in ['/system/common/lib','/system/priv/lib']:
 try:
  f.cwd(path);lines=[];f.retrlines('LIST',lines.append)
  result[path]=[s for s in lines if any(v in s.lower() for v in ['avplayer','videodec','audiodec','videocore'])]
 except Exception as e:result[path]=str(e)
f.quit();(p/'avplayer-module-locations-103.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
