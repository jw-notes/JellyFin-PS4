from pathlib import Path
from urllib.request import Request,urlopen
import json,uuid
p=Path(__file__).resolve().parent.parent/'build/diagnostics-104';p.mkdir(parents=True,exist_ok=True)
base='http://192.168.1.141:8097'
device='ps4-local-diagnostics-'+uuid.uuid4().hex
headers={'Authorization':f'MediaBrowser Client="Jellyfin PS4 Diagnostics", Device="Local PC", DeviceId="{device}", Version="0.5.0"','Content-Type':'application/json'}
r=json.load(urlopen(Request(base+'/QuickConnect/Initiate',data=b'{}',headers=headers),timeout=15))
(p/'pending.json').write_text(json.dumps({'base':base,'headers':headers,'secret':r['Secret'],'code':r['Code']}))
print('Quick Connect approval code:',r['Code'])
