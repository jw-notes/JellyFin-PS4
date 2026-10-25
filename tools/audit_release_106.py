from pathlib import Path
from PIL import Image
import csv,hashlib,json,re
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(r['Path']).read_bytes()).hexdigest().upper()==r['Hash'] for r in rows)
for n in ('103','104','105'):
 prior=json.loads((p/f'logs/results-{n}.json').read_text())
 assert hashlib.sha256((p/prior['pkg']).read_bytes()).hexdigest().upper()==prior['sha256']
def log(name):return (p/'logs'/name).read_text(encoding='utf-16')
assert log('build-106.log').count('[OK]')==28
assert 'PASS actual app loop' in log('test-106.log') and 'PASS real HTTP response' in log('test-106.log')
assert 'PASS real Jellyfin' in log('test-integration-106.log')
assert 'PASS shared' in log('test-ui-106.log')
for name in ('search-navigation-106','settings-navigation-106'):Image.open(p/'build'/f'{name}.bmp').save(p/'build'/f'{name}.png')
pkg=p/'dist/v1.06/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
result={'version':'01.06','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'ps3_hashes_unchanged':len(rows),'previous_releases_103_104_105_unchanged':True,'navigation_test':'actual production application loop, real renderer/input filter and local Jellyfin API; scripted host controller/display only','large_library_test':'real HTTP response exceeding 384 KiB; 60 items and preview metadata parsed','integration':'real isolated local Jellyfin movie HLS video/audio decoding, browsing and playback reporting passed','native_decoder':'not confirmed fixed; latest PS4 log AddSource=0x806a0002; native logging and missing same-server token handling added','console_operations':'read-only FTP log retrieval only'}
(p/'logs/results-106.json').write_text(json.dumps(result,indent=2))
f=p/'README.md';t=f.read_text().replace('**v1.05**. See [release notes](RELEASE-1.05.md). The new package is in `dist/v1.05/`','**v1.06**. See [release notes](RELEASE-1.06.md). The new package is in `dist/v1.06/`').replace('- PKG: `dist/v1.05/','- PKG: `dist/v1.06/').replace('application version `01.05`','application version `01.06`')
old=json.loads((p/'logs/results-105.json').read_text());t=t.replace(old['sha256'],result['sha256']);t=re.sub(r'- Size: [\d,]+ bytes\.', '- Size: '+format(result['bytes'],',')+' bytes.',t)
t=t.replace('## Implemented\n','## Implemented\n\n- L1/R1 opens Home, Search and Settings in either direction. Left/Right at list ends changes media pages. The PS4 JSON budget is 8 MiB and library listings omit unused media-source data.\n')
t=t.replace('but needs a console playback retest.','but the latest console log still reports AddSource 0x806a0002. Version 1.06 adds native decoder diagnostics and missing same-server URL authentication; playback is not confirmed fixed.')
f.write_text(t,newline='\n')
print(json.dumps(result,indent=2))
