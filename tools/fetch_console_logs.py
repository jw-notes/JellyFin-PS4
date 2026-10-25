from ftplib import FTP,error_perm
from pathlib import Path
from datetime import datetime,timezone
import json,posixpath
p=Path(__file__).resolve().parent.parent
out=p/'logs/console'/datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True,exist_ok=False)
f=FTP();f.connect('192.168.1.26',2121,timeout=15);f.login()
listings={};files=[]
def listing(path):
 rows=[]
 try:
  f.cwd(path)
  f.retrlines('LIST',rows.append)
 except Exception as e:rows.append(str(e))
 listings[path]=rows
 return rows
# New builds mirror diagnostic messages only into a public log directory.
# Credentials and saved sessions remain in the private application directory.
listing('/data/JellyFin-PS4-logs')
try:
 f.cwd('/data/JellyFin-PS4-logs')
 for filename in f.nlst():
  name=posixpath.basename(filename)
  if name.startswith('user-') and name.endswith('.log'):
   local=out/name
   with local.open('wb') as dest:f.retrbinary('RETR '+name,dest.write)
   files.append(str(local.relative_to(p)))
except error_perm:pass
listing('/data/JellyFin-PS4-logs/screenshots')
try:
 f.cwd('/data/JellyFin-PS4-logs/screenshots')
 for filename in sorted(f.nlst())[-6:]:
  name=posixpath.basename(filename)
  if name.startswith('screen-') and name.endswith('.bmp'):
   with (out/name).open('wb') as dest:f.retrbinary('RETR '+name,dest.write)
   files.append(str((out/name).relative_to(p)))
except error_perm:pass
listing('/data/JellyFin-PS4')
try:
 f.cwd('/data/JellyFin-PS4')
 entries=f.nlst()
 for entry in entries:
  name=posixpath.basename(entry.rstrip('/'))
  if not name.startswith('user-'):continue
  remote='/data/JellyFin-PS4/'+name
  listing(remote)
  try:
   f.cwd(remote)
   for file in f.nlst():
    filename=posixpath.basename(file)
    if filename.endswith('.log') or filename.endswith('.txt') and ('log' in filename.lower() or 'crash' in filename.lower()):
     local=out/(name+'-'+filename)
     with local.open('wb') as dest:f.retrbinary('RETR '+filename,dest.write)
     files.append(str(local.relative_to(p)))
  except error_perm as e:listings[remote].append(str(e))
except error_perm as e:print(e)
for path in ['/data/sce_logs','/data/sce_coredumps','/data/GoldHEN']:
 listing(path)
f.quit()
(out/'ftp-readonly-report.json').write_text(json.dumps({'host':'192.168.1.26:2121','operations':'CWD navigation, LIST, NLST, RETR only','listings':listings,'downloaded_logs':files},indent=2))
print(json.dumps({'directory':str(out),'files':files,'listings':listings},indent=2))
