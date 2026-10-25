from pathlib import Path
from PIL import Image, ImageChops
import csv, hashlib, json, re
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(r['Path']).read_bytes()).hexdigest().upper()==r['Hash'] for r in rows)
previous={'dist/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'61F9A12B642DC3E4798DAC1220F6112DA94361F32312706153788E0C54A95B81','dist/v1.01/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'CB045BCFF2D2C7A980A58572850AF65F7AD1B412085A9805FF52D1B3F22260DA','dist/v1.02/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'7801BFA3C16A8EE3E34BA98121B9A455031CBE645E213A16DF95E8F7C05F4D2B','dist/v1.03/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'043876B2BFF45DDE27D17740D07B3BC7D445A183A87FAD148B20ED00F379FA01'}
for path,digest in previous.items():assert hashlib.sha256((p/path).read_bytes()).hexdigest().upper()==digest
original=Image.open(p/'vendor/ps3/ICON0.PNG').convert('RGBA');original.thumbnail((512,512),Image.Resampling.LANCZOS)
expected=Image.new('RGBA',(512,512),(0,0,0,255));expected.alpha_composite(original,((512-original.width)//2,(512-original.height)//2))
assert ImageChops.difference(expected,Image.open(p/'build/pkg/sce_sys/icon0.png').convert('RGBA')).getbbox() is None
for image in (p/'assets/prompts').glob('*.png'):
 assert image.read_bytes()==(p/'build/pkg/assets/prompts'/image.name).read_bytes()
assert len(list((p/'build/pkg/assets/prompts').glob('*.png')))==9
assert list(Image.open(p/'build/screenshot-encoder-test.bmp').convert('RGB').get_flattened_data())==[(255,0,0),(0,255,0),(0,0,255),(255,255,255)]
build=(p/'logs/build-104-final.log').read_text(encoding='utf-16')
assert build.count('[OK]')==28
pkg=p/'dist/v1.04/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
results={'version':'01.04','title':'Jellyfin [PS]','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'ps3_hashes_unchanged':len(rows),'previous_pkgs_unchanged':True,'packaged_controller_icons':9,'native_PS4_playback_retest':'required; local FFmpeg decoding does not validate native AvPlayer','console_operations':'none during this update; earlier diagnostics were read-only FTP','user_server_sign_in':'not performed; no diagnostic Quick Connect approval required'}
(p/'logs/results-104.json').write_text(json.dumps(results,indent=2))
f=p/'README.md';t=f.read_text();t=t.replace('**v1.03**. See [release notes](RELEASE-1.03.md). The new package is in `dist/v1.03/`','**v1.04**. See [release notes](RELEASE-1.04.md). The new package is in `dist/v1.04/`').replace('- PKG: `dist/v1.03/','- PKG: `dist/v1.04/').replace('application version `01.03`','application version `01.04`').replace(previous['dist/v1.03/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'],results['sha256'])
t=re.sub(r'- Size: [\d,]+ bytes\.', '- Size: '+format(results['bytes'],',')+' bytes.',t)
t=t.replace('AvPlayer loading and the new layout in v1.03 still need a console retest.','The console logs confirm AvPlayer loads; stream opening failed in v1.03. Version 1.04 supplies native HTTP/SSL contexts and explicit HLS source metadata, but needs a console playback retest.')
t=t.replace('## Local verification\n','## Local verification\n\n- v1.04: `logs/build-104-final.log`, `logs/test-104-final.log`, `logs/test-integration-104.log`, `logs/test-auth-104.log` and `logs/test-ui-104.log`. Real isolated Jellyfin 10.10.7 movie and Live TV HLS video/audio decoding passed locally.\n- `build/library-preview-104.png` and `build/detail-preview-104.png` show the shared renderer with public server artwork and host-only preview metadata; these are not PS4 captures.\n')
f.write_text(t,newline='\n')
print(json.dumps(results,indent=2))
