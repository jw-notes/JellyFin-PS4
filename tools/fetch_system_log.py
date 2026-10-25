from ftplib import FTP
from pathlib import Path
from datetime import datetime,timezone
import json
p=Path(__file__).resolve().parent.parent
out=p/'logs/console'/datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True,exist_ok=False)
f=FTP();f.connect('192.168.1.26',2121,timeout=20);f.login();f.cwd('/data/sce_logs')
with (out/'sce-log.bin').open('wb') as dest:f.retrbinary('RETR log.bin',dest.write)
results={}
for path in ['/system_data/priv/errlog','/user/data','/data/JellyFin-PS4/','/data/JellyFin-PS4/.']:
 try:
  f.cwd(path);rows=[];f.retrlines('LIST',rows.append);results[path]=rows
 except Exception as e:results[path]=str(e)
f.quit()
(out/'read-only-locations.json').write_text(json.dumps(results,indent=2))
print('Downloaded system log to',out)
print(json.dumps(results,indent=2))
