from pathlib import Path
import re,json
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4/logs/console/20261024-173217')
b=(p/'sce-log.bin').read_bytes()
patterns=[rb'JFPS40000',rb'Jelly[Ff]in',rb'SIGSEGV',rb'SIGABRT',rb'CE-34878',rb'abnormal',rb'LoadExec',rb'crash',rb'exception']
matches=[]
for pattern in patterns:
 for m in list(re.finditer(pattern,b,re.I))[-25:]:
  context=b[max(0,m.start()-150):min(len(b),m.end()+250)]
  matches.append({'pattern':pattern.decode(),'offset':m.start(),'context':''.join(chr(c) if 32<=c<127 or c in [10,13] else '.' for c in context)})
result={'bytes':len(b),'nonzero_bytes':sum(c!=0 for c in b),'header_hex':b[:64].hex(),'matches':matches}
(p/'system-log-analysis.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2)[:14000])
