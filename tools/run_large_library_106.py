# JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
# GPL-3.0-or-later, with the attribution terms in NOTICE.
from http.server import BaseHTTPRequestHandler,HTTPServer
from urllib.parse import urlsplit,parse_qs
from pathlib import Path
import json,threading,subprocess
p=Path(__file__).resolve().parent.parent
items=[{'Id':str(n),'Name':f'Large library item {n}','Type':'Movie','Overview':'Large response synopsis. '*400,'ProductionYear':2026,'Genres':['Drama'],'People':[{'Type':'Actor','Name':'Actor One'},{'Type':'Director','Name':'Not cast'},{'Type':'Actor','Name':'Actor Two'}]} for n in range(60)]
body=json.dumps({'Items':items,'TotalRecordCount':121}).encode()
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  fields=parse_qs(urlsplit(self.path).query).get('Fields',[''])[0].split(',')
  assert 'MediaSources' not in fields and 'People' in fields
  self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=HTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:subprocess.run([str(p/'build/large-library-106'),f'http://127.0.0.1:{server.server_port}'],cwd=p,check=True,timeout=30)
finally:server.shutdown();server.server_close()
