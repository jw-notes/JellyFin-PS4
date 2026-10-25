from pathlib import Path
from urllib.request import urlopen
import json
root=Path(__file__).resolve().parent.parent/'logs/playback-references-104';root.mkdir(exist_ok=True)
for name in ['MediaInfoController.cs','LiveTvController.cs']:
 try:(root/name).write_bytes(urlopen('https://raw.githubusercontent.com/jellyfin/jellyfin/master/Jellyfin.Api/Controllers/'+name,timeout=15).read())
 except Exception as e:print(e)
try:
 data=json.load(urlopen('http://192.168.1.141:8097/api-docs/openapi.json',timeout=15))
 subset={k:v for k,v in data['paths'].items() if k in ['/LiveStreams/Open','/LiveStreams/Close','/Items/{itemId}/PlaybackInfo','/LiveTv/TunerHosts','/LiveTv/Guide/Refresh']}
 (root/'api-paths.json').write_text(json.dumps(subset,indent=2))
 for name in ['OpenLiveStreamDto','PlaybackInfoDto','TunerHostInfo']:
  schema=data.get('components',{}).get('schemas',{}).get(name,{})
  (root/(name+'.json')).write_text(json.dumps(schema,indent=2))
 print('Saved server API schemas',list(subset))
except Exception as e:print(e)
