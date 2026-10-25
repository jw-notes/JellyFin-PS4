from pathlib import Path
import subprocess,shutil,os,re,sys
p=Path(__file__).resolve().parent.parent
url=(p/(sys.argv[1] if len(sys.argv)>1 else 'build/test-playback-url.txt')).read_text().replace('127.0.0.1:18096','127.0.0.1:8096')
docker=shutil.which('docker') or shutil.which('docker.exe') or '/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe'
cmd=[docker,'exec','jellyfin-ps4-port-test','/usr/lib/jellyfin-ffmpeg/ffmpeg','-hide_banner','-loglevel','warning','-i',url,'-t','3','-map','0:v:0','-map','0:a:0','-f','null','-']
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
log=re.sub(r'api_key=[^&\s]+','api_key=[test-token]',r.stdout)
(p/('logs/stream-decode-live-104.log' if len(sys.argv)>1 else 'logs/stream-decode-exact.log')).write_text(log+'\nexit_code='+str(r.returncode)+'\n')
if r.returncode:print(log)
else:print('PASS: exact port-generated HLS URL decoded for 3 seconds with video and audio')
raise SystemExit(r.returncode)
