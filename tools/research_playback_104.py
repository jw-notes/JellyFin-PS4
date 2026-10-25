from pathlib import Path
from urllib.request import urlopen
p=Path(__file__).resolve().parent.parent/'logs/avplayer-references-104';p.mkdir(exist_ok=True)
for name in ['avplayer_impl.cpp','avplayer_error.h','avplayer.h']:
 try:(p/name).write_bytes(urlopen('https://raw.githubusercontent.com/shadps4-emu/shadPS4/main/src/core/libraries/avplayer/'+name,timeout=15).read())
 except Exception as e:print(e)
