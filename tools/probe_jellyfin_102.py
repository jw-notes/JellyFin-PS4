import urllib.request,urllib.error,json
url='http://192.168.1.141:8097/System/Info/Public'
try:
 r=urllib.request.urlopen(url,timeout=10)
 body=r.read(65536)
 print(json.dumps({'url':url,'status':r.status,'final_url':r.url,'body':body.decode(errors='replace')},indent=2))
except Exception as e: print(type(e).__name__+': '+str(e))
