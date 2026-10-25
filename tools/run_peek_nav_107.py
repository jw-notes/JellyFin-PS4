from pathlib import Path
import json,sys,urllib.request,subprocess,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlsplit
base,user,password=sys.argv[1:4]
assert base=='http://127.0.0.1:18096','Fixture updates are restricted to the isolated local test server'
headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="PS4 Preview Fixture", Device="localhost", DeviceId="ps4-preview-fixture-107", Version="1"'}
def req(path,body=None):
 r=urllib.request.Request(base+path,data=json.dumps(body).encode() if body is not None else None,headers=headers)
 with urllib.request.urlopen(r,timeout=25) as response:
  content=response.read();return json.loads(content) if content else None
auth=req('/Users/AuthenticateByName',{'Username':user,'Pw':password});headers['X-Emby-Token']=auth['AccessToken'];uid=auth['User']['Id']
movie=req('/Users/'+uid+'/Items?Recursive=true&SearchTerm=PS4%20Integration')['Items'][0]
original=req('/Users/'+uid+'/Items/'+movie['Id']);fixture=dict(original)
fixture.update(Overview='PS4 quick-peek integration synopsis.',People=[{'Name':'Preview Actor','Type':'Actor','Role':'Test character'}])
stripped=[]
class MissingListFactsProxy(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def forward(self):
  body=self.rfile.read(int(self.headers.get('Content-Length',0)))
  forwarded={k:v for k,v in self.headers.items() if k.lower() not in ('host','connection','content-length')}
  request=urllib.request.Request(base+self.path,data=body if self.command!='GET' else None,headers=forwarded,method=self.command)
  try:response=urllib.request.urlopen(request,timeout=30)
  except urllib.error.HTTPError as error:response=error
  with response:
   content=response.read();kind=response.headers.get('Content-Type','application/octet-stream')
   if self.command=='GET' and urlsplit(self.path).path.endswith('/Items') and response.status==200:
    payload=json.loads(content)
    for item in payload.get('Items',[]):item.pop('Overview',None);item.pop('People',None)
    content=json.dumps(payload).encode();stripped.append(self.path)
   self.send_response(response.status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
 do_GET=forward
 do_POST=forward
 do_DELETE=forward
proxy=ThreadingHTTPServer(('127.0.0.1',0),MissingListFactsProxy)
threading.Thread(target=proxy.serve_forever,daemon=True).start()
try:
 req('/Items/'+movie['Id'],fixture)
 subprocess.run([str(Path('build/navigation-107').resolve()),f'http://127.0.0.1:{proxy.server_port}',user,password],check=True,timeout=120)
 assert stripped,'The missing-list-metadata condition was not exercised'
finally:
 proxy.shutdown();proxy.server_close()
 req('/Items/'+movie['Id'],original)
 req('/Sessions/Logout',{})
