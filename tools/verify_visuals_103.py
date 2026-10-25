from pathlib import Path
from PIL import Image,ImageChops
p=Path(__file__).resolve().parent.parent
for f in (p/'build').glob('*preview-103.bmp'):Image.open(f).save(f.with_suffix('.png'))
original=Image.open(p/'vendor/ps3/ICON0.PNG').convert('RGBA');original.thumbnail((512,512),Image.Resampling.LANCZOS)
expected=Image.new('RGBA',(512,512),(0,0,0,255));expected.alpha_composite(original,((512-original.width)//2,(512-original.height)//2))
actual=Image.open(p/'build/pkg/sce_sys/icon0.png').convert('RGBA')
assert ImageChops.difference(expected,actual).getbbox() is None
image=Image.open(p/'build/screenshot-encoder-test.bmp').convert('RGB')
assert list(image.get_flattened_data())==[(255,0,0),(0,255,0),(0,0,255),(255,255,255)]
print('PASS packaged icon matches original PS3 pixels after proportional resizing; screenshot colours/orientation correct')
