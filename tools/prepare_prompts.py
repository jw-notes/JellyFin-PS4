from pathlib import Path
from PIL import Image
import re,io,json
p=Path(__file__).resolve().parent.parent
header=(p/'vendor/ps3/source/ui/fonts/ps_buttons_png.h').read_text()
match=re.search(r'unsigned char ps_buttons_png\[\]\s*=\s*\{(.*?)\};',header,re.S)
assert match
sheet=Image.open(io.BytesIO(bytes(int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})',match[1])))).convert('RGBA')
assert sheet.size==(1536,1536)
folder=p/'assets/prompts';folder.mkdir(parents=True,exist_ok=True)
tiles={'cross':(10,5),'circle':(11,7),'square':(10,11),'triangle':(9,1),'dpad':(9,3),'l1':(6,3),'r1':(6,11),'l2':(6,7),'r2':(5,3)}
for name,(row,col) in tiles.items():
 tile=sheet.crop((col*128,row*128,(col+1)*128,(row+1)*128));bounds=tile.getchannel('A').getbbox();assert bounds;tile.crop(bounds).save(folder/(name+'.png'))
(folder/'provenance.json').write_text(json.dumps({'source':'vendor/ps3/source/ui/fonts/ps_buttons_png.h','tile_mapping':'original PS3 ui_widgets.cpp PS_TILES','tiles':tiles},indent=2))
print('Prepared original controller-sheet prompt images')
