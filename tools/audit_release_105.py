from pathlib import Path
from PIL import Image
import csv,hashlib,json,re
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(r['Path']).read_bytes()).hexdigest().upper()==r['Hash'] for r in rows)
for n in ('103','104'):
 result=json.loads((p/f'logs/results-{n}.json').read_text())
 assert hashlib.sha256((p/result['pkg']).read_bytes()).hexdigest().upper()==result['sha256']
build=(p/'logs/build-105.log').read_text(encoding='utf-16');assert build.count('[OK]')==28
assert 'PASS shared' in (p/'logs/test-ui-105.log').read_text(encoding='utf-16')
assert 'PASS real Jellyfin' in (p/'logs/test-integration-105.log').read_text(encoding='utf-16')
for f in (p/'build').glob('*preview-105.bmp'):Image.open(f).save(f.with_suffix('.png'))
pkg=p/'dist/v1.05/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
results={'version':'01.05','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'ps3_hashes_unchanged':len(rows),'previous_releases_103_104_unchanged':True,'quick_peek':'Triangle opens, Cross opens details/folder; Triangle/Circle/directions/L1/R1 close without changing selection','original_code_reused':'vendor/ps3/source/ui/render/peek.h','verification':'native build, shared renderer overlay/turn previews, local real Jellyfin integration and movie HLS video/audio decode passed','console_changes':'none','PS4_retest':'required'}
(p/'logs/results-105.json').write_text(json.dumps(results,indent=2))
f=p/'README.md';t=f.read_text().replace('**v1.04**. See [release notes](RELEASE-1.04.md). The new package is in `dist/v1.04/`','**v1.05**. See [release notes](RELEASE-1.05.md). The new package is in `dist/v1.05/`').replace('- PKG: `dist/v1.04/','- PKG: `dist/v1.05/').replace('application version `01.04`','application version `01.05`')
old=json.loads((p/'logs/results-104.json').read_text());t=t.replace(old['sha256'],results['sha256']);t=re.sub(r'- Size: [\d,]+ bytes\.', '- Size: '+format(results['bytes'],',')+' bytes.',t)
t=t.replace('## Implemented\n','## Implemented\n\n- Triangle quick peek reuses the PS3 poster-flip animation and cached library metadata. Cross opens details/folders; Triangle, Circle or navigation closes the preview. Search is available in the top menu.\n')
f.write_text(t,newline='\n')
print(json.dumps(results,indent=2))
