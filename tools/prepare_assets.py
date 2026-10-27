from pathlib import Path
from PIL import Image
import re,json,hashlib
p=Path(__file__).resolve().parent.parent
assets=p/'assets';assets.mkdir(exist_ok=True)
# The app icon: the Jellyfin [PS] brand mark (the same artwork as
# assets/jellyfin-mark.png, from its 256 px master so it stays sharp at
# 512 px) on the interface's own dark gradient.
import io
hd=(p/'vendor/ps3/source/gfx/jfmark_hd_png.h').read_text()
hd_match=re.search(r'unsigned char jfmark_hd_cool_png\[\]\s*=\s*\{(.*?)\};',hd,re.S)
assert hd_match
source_bytes=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',hd_match[1]))
mark=Image.open(io.BytesIO(source_bytes)).convert('RGBA').resize((400,400),Image.Resampling.LANCZOS)
canvas=Image.new('RGBA',(512,512))
top,bottom=(0x15,0x1A,0x38),(0x05,0x06,0x0C)
for y in range(512):
 t=y/511;canvas.paste(tuple(round(a+(b-a)*t) for a,b in zip(top,bottom))+(255,),(0,y,512,y+1))
canvas.alpha_composite(mark,(56,56));canvas.convert('RGB').save(assets/'icon0.png')
header=(p/'vendor/ps3/source/gfx/jfmark_png.h').read_text()
match=re.search(r'unsigned char jfmark_cool_png\[\]\s*=\s*\{(.*?)\};',header,re.S)
assert match
(assets/'jellyfin-mark.png').write_bytes(bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',match[1])))
(assets/'provenance.json').write_text(json.dumps({'icon_source':'vendor/ps3/source/gfx/jfmark_hd_png.h:jfmark_hd_cool_png','icon_source_sha256':hashlib.sha256(source_bytes).hexdigest(),'icon_processing':'256 px brand mark resized to 400 px (Lanczos), centred on a 512x512 #151A38 to #05060C vertical gradient','brand_mark_source':'vendor/ps3/source/gfx/jfmark_png.h:jfmark_cool_png'},indent=2))
print('Prepared the brand-mark app icon and the original Jellyfin brand mark')
