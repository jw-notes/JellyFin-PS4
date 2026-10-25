from http.server import BaseHTTPRequestHandler,HTTPServer
from urllib.parse import urlsplit,parse_qs
from pathlib import Path
import json,threading,subprocess
seen=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def reply(self,data,status=200):
  payload=json.dumps(data).encode();self.send_response(status);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
 def do_GET(self):
  assert self.path.startswith('/Items/channel-id/PlaybackInfo?');seen.append('catalogue')
  self.reply({'PlaySessionId':'catalogue-session','MediaSources':[{'Id':'catalogue-source','RequiresOpening':True,'OpenToken':'fixture-open-token'}]})
 def do_POST(self):
  raw=self.rfile.read(int(self.headers.get('Content-Length',0)));body=json.loads(raw) if raw else {}
  if self.path.startswith('/Items/channel-id/PlaybackInfo?'):
   assert 'MediaSourceId' not in body and 'AudioStreamIndex' not in body
   seen.append('profile');self.reply({'MediaSources':[],'ErrorCode':'NoCompatibleStream','PlaySessionId':'initial-session'})
  elif self.path=='/LiveStreams/Open':
   assert body['OpenToken']=='fixture-open-token' and body['PlaySessionId']=='catalogue-session'
   seen.append('open');self.reply({'MediaSource':{'Id':'opened-source','LiveStreamId':'opened-live','SupportsTranscoding':True}})
  elif self.path.startswith('/LiveStreams/Close?'):
   assert parse_qs(urlsplit(self.path).query)['LiveStreamId']==['opened-live'];seen.append('close');self.reply({})
  else:raise AssertionError(self.path)
 def do_DELETE(self):seen.append('stop-transcode');self.reply({})
server=HTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 subprocess.run([str(Path('build/live-fallback-107').resolve()),f'http://127.0.0.1:{server.server_port}'],check=True,timeout=30)
 assert seen==['profile','catalogue','open','stop-transcode','close'],seen
finally:server.shutdown();server.server_close()
