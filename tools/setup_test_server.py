import json,time,urllib.request,urllib.error,pathlib
root=pathlib.Path(__file__).resolve().parent.parent
base='http://127.0.0.1:18096'
def req(path,body=None,token=None):
 data=json.dumps(body).encode() if body is not None else None
 headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="PS4 Port Test", Device="localhost", DeviceId="ps4-port-test-harness", Version="1"'}
 if token:headers['X-Emby-Token']=token
 r=urllib.request.urlopen(urllib.request.Request(base+path,data=data,headers=headers),timeout=60)
 b=r.read()
 return json.loads(b) if b else None
for n in range(60):
 try:req('/System/Info/Public');break
 except Exception:time.sleep(1)
info=req('/System/Info/Public')
if not info['StartupWizardCompleted']:
 req('/Startup/Configuration',{'UICulture':'en-GB','MetadataCountryCode':'GB','PreferredMetadataLanguage':'en'})
 req('/Startup/User')
 req('/Startup/User',{'Name':'ps4test','Password':'LocalPortTest-2026!'})
 req('/Startup/RemoteAccess',{'EnableRemoteAccess':False,'EnableAutomaticPortMapping':False})
 req('/Startup/Complete',{})
session=req('/Users/AuthenticateByName',{'Username':'ps4test','Pw':'LocalPortTest-2026!'})
token=session['AccessToken']
config=req('/System/Configuration',token=token)
config['MinResumeDurationSeconds']=60
req('/System/Configuration',config,token)
libs=req('/Library/VirtualFolders',token=token)
if not libs:
 req('/Library/VirtualFolders?name=Movies&collectionType=movies&refreshLibrary=true',{'LibraryOptions':{'PathInfos':[{'Path':'/media'}],'EnableRealtimeMonitor':False,'EnableInternetProviders':False,'EnableAutomaticSeriesGrouping':False}},token)
 req('/Library/Refresh',{},token)
for n in range(60):
 items=req('/Items?Recursive=true&IncludeItemTypes=Movie&SearchTerm=PS4%20Integration',token=token)
 if items.get('Items'):break
 time.sleep(1)
assert items.get('Items'),items
print('Real Jellyfin',info['Version'],'ready with',len(items['Items']),'test movie(s)')
(root/'logs/test-server.json').write_text(json.dumps({'version':info['Version'],'base':base,'movies':len(items['Items'])},indent=2))
