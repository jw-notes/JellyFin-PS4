from pathlib import Path
import json,sys,threading,urllib.request,urllib.error,subprocess
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlsplit,parse_qs
base,user,password=sys.argv[1:4];assert base=='http://127.0.0.1:18096'
headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="PS4 Page Fixture", Device="localhost", DeviceId="ps4-page-fixture-108", Version="1"'}
def req(path,body=None):
 with urllib.request.urlopen(urllib.request.Request(base+path,headers=headers,data=json.dumps(body).encode() if body is not None else None),timeout=20) as r:
  body=r.read();return json.loads(body) if body else None
auth=req('/Users/AuthenticateByName',{'Username':user,'Pw':password});headers['X-Emby-Token']=auth['AccessToken'];uid=auth['User']['Id']
movie=req('/Users/'+uid+'/Items?Recursive=true&SearchTerm=PS4%20Integration')['Items'][0]
items=[dict(movie,Name=f'Fixture movie {i:03}',Overview='Host-only pagination fixture') for i in range(130)];pages=[]
class Proxy(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def forward(self):
  body=self.rfile.read(int(self.headers.get('Content-Length',0)));query=parse_qs(urlsplit(self.path).query)
  if self.command=='GET' and urlsplit(self.path).path.endswith('/Items') and 'ParentId' in query:
   start=int(query.get('StartIndex',['0'])[0]);pages.append(start);payload=json.dumps({'Items':items[start:start+60],'TotalRecordCount':130}).encode();status=200;kind='application/json'
  else:
   h={k:v for k,v in self.headers.items() if k.lower() not in ('host','connection','content-length')}
   request=urllib.request.Request(base+self.path,headers=h,data=body if self.command!='GET' else None,method=self.command)
   try:response=urllib.request.urlopen(request,timeout=30)
   except urllib.error.HTTPError as error:response=error
   with response:payload=response.read();status=response.status;kind=response.headers.get('Content-Type','application/octet-stream')
  self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
 do_GET=forward
 do_POST=forward
 do_DELETE=forward
server=ThreadingHTTPServer(('127.0.0.1',0),Proxy);threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 subprocess.run([str(Path('build/pagination-108').resolve()),f'http://127.0.0.1:{server.server_port}',user,password],check=True,timeout=120)
 assert pages==[0,60,0,60],pages
 print('PASS real app requested page offsets 0 -> 60 -> 0 -> 60 through vertical grid navigation')
finally:server.shutdown();server.server_close();req('/Sessions/Logout',{})
