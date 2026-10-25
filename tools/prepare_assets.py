from pathlib import Path
from PIL import Image
import re,json,hashlib
p=Path(__file__).resolve().parent.parent
assets=p/'assets';assets.mkdir(exist_ok=True)
source=p/'vendor/ps3/ICON0.PNG'
image=Image.open(source).convert('RGBA');image.thumbnail((512,512),Image.Resampling.LANCZOS)
canvas=Image.new('RGBA',(512,512),(0,0,0,255));canvas.alpha_composite(image,((512-image.width)//2,(512-image.height)//2));canvas.save(assets/'icon0.png')
header=(p/'vendor/ps3/source/gfx/jfmark_png.h').read_text()
match=re.search(r'unsigned char jfmark_cool_png\[\]\s*=\s*\{(.*?)\};',header,re.S)
assert match
(assets/'jellyfin-mark.png').write_bytes(bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',match[1])))
(assets/'provenance.json').write_text(json.dumps({'icon_source':'vendor/ps3/ICON0.PNG','icon_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'icon_processing':'original pixels resized proportionally and centered on a 512x512 black canvas','brand_mark_source':'vendor/ps3/source/gfx/jfmark_png.h:jfmark_cool_png'},indent=2))
print('Prepared original PS3 icon and original Jellyfin brand mark')
