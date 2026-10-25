from pathlib import Path
from PIL import Image,ImageChops
import csv,hashlib,json,re
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(r['Path']).read_bytes()).hexdigest().upper()==r['Hash'] for r in rows)
for n in ('103','104','105','106'):
 old=json.loads((p/f'logs/results-{n}.json').read_text());assert hashlib.sha256((p/old['pkg']).read_bytes()).hexdigest().upper()==old['sha256']
def read(name):
 f=p/'logs'/name;raw=f.read_bytes();return raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8')
assert read('build-107-final.log').count('[OK]')==28
assert 'PASS real Live TV through production MPV adapter' in read('test-player-107-final.log')
assert 'PASS movie pause, resume, seek' in read('test-player-107-final.log')
assert 'PASS quick peek from empty list metadata' in read('test-navigation-107-final.log')
assert 'PASS NoCompatibleStream recovery' in read('test-fallback-107.log')
assert 'rapid taps' in read('test-input-live-107.log')
assert 'videodec2: sceKernelAllocateDirectMemory' in read('ps4-player-binary-107.txt')
assert 'build 01.07 (MPV/FFmpeg)' in read('ps4-player-binary-107.txt')
original=Image.open(p/'vendor/ps3/ICON0.PNG').convert('RGBA');original.thumbnail((512,512),Image.Resampling.LANCZOS)
expected=Image.new('RGBA',(512,512),(0,0,0,255));expected.alpha_composite(original,((512-original.width)//2,(512-original.height)//2))
assert ImageChops.difference(expected,Image.open(p/'build/pkg/sce_sys/icon0.png').convert('RGBA')).getbbox() is None
Image.open(p/'build/quick-peek-live-metadata-107.bmp').save(p/'build/quick-peek-live-metadata-107.png')
pkg=p/'dist/v1.07/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
result={'version':'01.07','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'playback_backend':'MPV 0.36.0 / FFmpeg 7.1.5, Switchfin PS4 packages; real Videodec2 implementation linked','local_player_test':'production adapter: changing movie and Live TV frames, audio decoding, clock, pause/resume/seek, asynchronous errors','controller_and_peek':'actual app loop; D-pad/trigger combinations, all tabs, settings, exit and full item metadata despite missing list fields','live_recovery':'NoCompatibleStream -> source catalogue -> OpenToken -> authenticated opened-source HLS -> cleanup','PS4_hardware_retest':'required; no console installation performed','ps3_hashes_unchanged':len(rows),'previous_releases_103_104_105_106_unchanged':True,'original_icon_pixels_unchanged':True,'console_operations':'read-only FTP logs and runtime module file-size checks only'}
(p/'logs/results-107.json').write_text(json.dumps(result,indent=2))
f=p/'README.md';t=f.read_text();start=t.index('## Build result');end=t.index('## Implemented',start)
t=t[:start]+'''## Build result

- Latest package: `dist/v1.07/IV0000-JFPS40000_00-JELLYFINPS400000.pkg`.
- Title ID `JFPS40000`, application version `01.07`, title **Jellyfin [PS]**.
- Size: '''+format(result['bytes'],',')+''' bytes. SHA256: `'''+result['sha256']+'''`.
- Native PS4 ELF/SELF/PKG built; all 28 package checks passed. The MPV/FFmpeg production adapter passed real local movie/Live TV playback tests. Controller and missing-preview-metadata regressions passed through the actual app loop.
- **PS4 hardware playback/audio still needs a console retest.** Version 1.07 replaces the failing AvPlayer demux path with the Switchfin PS4 MPV/FFmpeg backend. No console installation or modification was performed.

'''+t[end:]
t=re.sub(r'Latest update:.*', 'Latest update: **v1.07**. See [release notes](RELEASE-1.07.md) and [playback attribution](NOTICE-PS4-PLAYBACK.md). Prior packages are preserved.',t,count=1)
t=t.replace('## Implemented\n','''## Implemented

- MPV/FFmpeg playback from Switchfin's PS4 branch, with H264 Videodec2 preferred and software fallback. Current framebuffer output is 720p and audio is stereo at 48 kHz. See the current release notes for the verified behavior and remaining hardware retest.
- Quick peek retrieves full metadata asynchronously; Home has explicit Continue/Favourites/Live TV destinations; L2/R2 no longer opens global shortcut views. Rapid taps and grid boundaries are corrected.
''')
t=t.replace('## Local verification\n','''## Local verification

- Current: `logs/build-107-final.log`, `logs/test-player-107-final.log`, `logs/test-navigation-107-final.log`, `logs/test-fallback-107.log` and `logs/results-107.json`.
- `build/quick-peek-live-metadata-107.png` is a host-rendered capture from the real local-server test with list metadata deliberately omitted, not a PS4 capture.
''')
f.write_text(t,newline='\n')
print(json.dumps(result,indent=2))
