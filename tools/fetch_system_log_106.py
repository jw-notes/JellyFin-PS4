from ftplib import FTP
from pathlib import Path
import re
out=Path('logs/console/20261024-191250')
f=FTP();f.connect('192.168.1.26',2121,timeout=20);f.login()
with (out/'system-log.bin').open('wb') as dest:f.retrbinary('RETR /data/sce_logs/log.bin',dest.write)
f.quit()
parts=re.findall(rb'[\x20-\x7e\r\n\t]{8,}',(out/'system-log.bin').read_bytes())
text=b'\n'.join(parts).decode('ascii')
(out/'system-printable.txt').write_text(text)
lines=[s for s in text.splitlines() if re.search(r'avplayer|806a|demux|decoder|ssl|http',s,re.I)]
(out/'system-media-lines.txt').write_text('\n'.join(lines))
print('Downloaded system log read-only; matching media/network lines:',len(lines))
