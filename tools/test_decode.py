import json,urllib.request,subprocess,pathlib,urllib.parse
p=pathlib.Path(__file__).resolve().parent.parent
base='http://127.0.0.1:18096'
headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="PS4 Decode Test", Device="localhost", DeviceId="ps4-port-decode", Version="1"'}
def req(path,body=None):
 r=urllib.request.urlopen(urllib.request.Request(base+path,data=json.dumps(body).encode() if body is not None else None,headers=headers),timeout=60)
 b=r.read();return json.loads(b) if b else None
s=req('/Users/AuthenticateByName',{'Username':'ps4test','Pw':'LocalPortTest-2026!'})
headers['X-Emby-Token']=s['AccessToken']
items=req('/Items?Recursive=true&IncludeItemTypes=Movie&SearchTerm=PS4%20Integration')['Items'];item=items[0]['Id']
profile={'Name':'JellyFin PS4','MaxStreamingBitrate':8000000,'DirectPlayProfiles':[],'TranscodingProfiles':[{'Container':'ts','Type':'Video','VideoCodec':'h264','AudioCodec':'aac','Protocol':'hls','Context':'Streaming','MaxAudioChannels':'2','MinSegments':'2','BreakOnNonKeyFrames':True}]}
info=req('/Items/'+item+'/PlaybackInfo',{'UserId':s['User']['Id'],'DeviceProfile':profile,'EnableDirectPlay':False,'EnableDirectStream':False,'EnableTranscoding':True,'MaxStreamingBitrate':8000000})
url=info['MediaSources'][0]['TranscodingUrl']
full='http://127.0.0.1:8096'+url
command=['docker','exec','jellyfin-ps4-port-test','/usr/lib/jellyfin-ffmpeg/ffmpeg','-hide_banner','-loglevel','warning','-i',full,'-t','3','-map','0:v:0','-map','0:a:0','-f','null','-']
r=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
log=r.stdout.replace(s['AccessToken'],'[test-token]')
(p/'logs/stream-decode.log').write_text(log+'\nexit_code='+str(r.returncode)+'\n')
assert r.returncode==0,log
req('/Sessions/Logout',{})
print('PASS: real Jellyfin HLS decoded for 3 seconds with both video and audio using FFmpeg')
