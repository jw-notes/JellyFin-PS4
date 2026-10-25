from urllib.request import urlopen
from pathlib import Path
p=Path(__file__).resolve().parent.parent/'logs/avplayer-references-103'
for name in ['kernel','process']:
 u='https://raw.githubusercontent.com/shadps4-emu/shadPS4/main/src/core/libraries/kernel/'+name+'.cpp'
 try:(p/(name+'.cpp')).write_bytes(urlopen(u,timeout=15).read())
 except Exception as e:print(e)
