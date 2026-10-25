from pathlib import Path
from urllib.request import Request,urlopen
import json,subprocess,time
p=Path(__file__).resolve().parent.parent
base='http://127.0.0.1:18096'
headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="PS4 Live Test", Device="localhost", DeviceId="ps4-live-test-104", Version="1"'}
def req(path,body=None):
 r=urlopen(Request(base+path,data=json.dumps(body).encode() if body is not None else None,headers=headers),timeout=60);data=r.read();return json.loads(data) if data else None
session=req('/Users/AuthenticateByName',{'Username':'ps4test','Pw':'LocalPortTest-2026!'})
headers['X-Emby-Token']=session['AccessToken']
# The real transport stream is served only inside the disposable test container.
runner='while true; do /usr/lib/jellyfin-ffmpeg/ffmpeg -hide_banner -loglevel error -re -stream_loop -1 -i "/media/PS4 Integration (2026).mp4" -c copy -f mpegts -listen 1 http://0.0.0.0:18097/channel.ts; sleep 1; done'
subprocess.run(['docker','exec','-d','jellyfin-ps4-port-test','sh','-c',runner],check=True)
playlist=p/'build/server-config/port-test-live.m3u'
playlist.write_text('#EXTM3U\n#EXTINF:-1 tvg-id="ps4-live-test-104",PS4 Port Live Test\nhttp://127.0.0.1:18097/channel.ts\n')
existing=req('/System/Configuration/livetv').get('TunerHosts',[])
if not any(v.get('Url')=='/config/port-test-live.m3u' for v in existing):
 req('/LiveTv/TunerHosts',{'Type':'m3u','Url':'/config/port-test-live.m3u','FriendlyName':'PS4 Port Live Test','EnableAllTuners':True,'TunerCount':2})
try:req('/LiveTv/Guide/Refresh',{})
except Exception:pass
for n in range(60):
 channels=req('/LiveTv/Channels?UserId='+session['User']['Id'])
 matches=[i for i in channels.get('Items',[]) if i['Name']=='PS4 Port Live Test']
 if matches:break
 time.sleep(1)
assert matches,channels
(p/'logs/live-test-fixture-104.json').write_text(json.dumps({'channel_id':matches[0]['Id'],'name':matches[0]['Name'],'type':matches[0]['Type'],'fixture':'real FFmpeg MPEG-TS loop in isolated local Jellyfin container'},indent=2))
req('/Sessions/Logout',{})
print('Real isolated Jellyfin Live TV channel ready:',matches[0]['Id'])
