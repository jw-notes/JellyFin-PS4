from pathlib import Path
from PIL import Image,ImageChops
import csv,hashlib,json,re
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(r['Path']).read_bytes()).hexdigest().upper()==r['Hash'] for r in rows)
previous={'dist/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'61F9A12B642DC3E4798DAC1220F6112DA94361F32312706153788E0C54A95B81','dist/v1.01/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'CB045BCFF2D2C7A980A58572850AF65F7AD1B412085A9805FF52D1B3F22260DA','dist/v1.02/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'7801BFA3C16A8EE3E34BA98121B9A455031CBE645E213A16DF95E8F7C05F4D2B'}
for n in range(103,108):
 old=json.loads((p/f'logs/results-{n}.json').read_text());previous[old['pkg']]=old['sha256']
for path,digest in previous.items():assert hashlib.sha256((p/path).read_bytes()).hexdigest().upper()==digest
def read(name):
 raw=(p/'logs'/name).read_bytes();return raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8')
assert read('build-108-release.log').count('[OK]')==28
assert 'PASS original PS3 ClassicGrid' in read('test-classic-108.log')
assert 'PASS fatal signal log' in read('test-classic-108.log')
assert 'PASS real Live TV through production MPV adapter' in read('test-player-108.log')
assert 'PASS quick peek' in read('test-navigation-108.log')
assert 'PASS QWERTY layout' in read('test-keyboard-108.log')
assert 'PASS real app requested page offsets 0 -> 60 -> 0 -> 60' in read('test-pagination-108.log')
binary=(p/'build/JellyFin-PS4.elf').read_bytes()
assert b'Jellyfin [PS] PS4 build 01.08' in binary
assert b'MPV: opening stream with FFmpeg software decoding; hardware decoding disabled' in binary
assert b'MaxWidth=1280&MaxHeight=720' in binary
for f in (p/'build').glob('classic-*-108.bmp'):Image.open(f).save(f.with_suffix('.png'))
original=Image.open(p/'vendor/ps3/ICON0.PNG').convert('RGBA');original.thumbnail((512,512),Image.Resampling.LANCZOS)
expected=Image.new('RGBA',(512,512),(0,0,0,255));expected.alpha_composite(original,((512-original.width)//2,(512-original.height)//2))
assert ImageChops.difference(expected,Image.open(p/'build/pkg/sce_sys/icon0.png').convert('RGBA')).getbbox() is None
pkg=p/'dist/v1.08/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
result={'version':'01.08','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'classic_browsing':'original PS3 ClassicGrid helper, Satoshi/Roboto Condensed fonts, 7x2 portrait posters, 3x2 wide episodes, row scrolling and breadcrumbs','decoder':'software H264 preferred, hwdec=no, six threads; H264/AAC transcode request capped at 1280x720','crash_evidence':'user confirms PS4 system error closed 1.07; log stops after MPV load; root cause not proven','native_retest':'required; crash fix not verified on PS4','local_tests':['real movie/Live TV MPV decoding and clock','pause/resume/seek and source errors','classic geometry/fonts/scroll/peek rendering','fatal signal diagnostic preserving signal','actual app-loop controls and preview metadata','working keyboard regression'],'ps3_hashes_unchanged':len(rows),'previous_pkgs_unchanged':len(previous),'original_icon_pixels_unchanged':True,'console_operations':'read-only FTP log and screenshot retrieval only'}
(p/'logs/results-108.json').write_text(json.dumps(result,indent=2))
f=p/'README.md';t=f.read_text();t=re.sub(r'Latest update:.*','Latest update: **v1.08**. See [release notes](RELEASE-1.08.md) and [playback attribution](NOTICE-PS4-PLAYBACK.md). Prior packages are preserved.',t,count=1)
start=t.index('## Build result');end=t.index('## Implemented',start)
t=t[:start]+'''## Build result

- Package: `dist/v1.08/IV0000-JFPS40000_00-JELLYFINPS400000.pkg`.
- Title ID `JFPS40000`, application version `01.08`, title **Jellyfin [PS]**.
- Size: '''+format(result['bytes'],',')+''' bytes. SHA256: `'''+result['sha256']+'''`.
- Native PS4 build and all 28 package checks passed. Local software movie/Live TV playback, classic layout, fonts, controls, preview metadata and keyboard tests passed.
- **The 1.07 console build crashed during playback.** Its exact cause is unknown. Version 1.08 disables the experimental hardware path and requests 720p transcodes; the PS4 crash fix still needs a console retest. No console installation/modification was performed.

'''+t[end:]
t=t.replace('- MPV/FFmpeg playback from Switchfin\'s PS4 branch, with H264 Videodec2 preferred and software fallback.','- MPV/FFmpeg playback from Switchfin\'s PS4 branch, currently using software H264 while the hardware crash is investigated.')
t=t.replace('- MPV asynchronous loading, FFmpeg demux/decoding, PS4 Videodec2 H264 preference, software fallback,','- MPV asynchronous loading, FFmpeg software demux/decoding,')
if '- Original PS3 classic grid geometry' not in t:t=t.replace('## Implemented\n','## Implemented\n\n- Original PS3 classic grid geometry and Satoshi/Roboto Condensed fonts: two-row movie/show/season posters, wide episode cards, breadcrumbs, compact metadata, row scrolling and the classic header.\n')
t=t.replace('Default H264/AAC HLS playback profile limited to 1080p','Default H264/AAC HLS playback profile limited to 720p')
t=t.replace('## Local verification\n','## Local verification\n\n- Current: `logs/build-108-release.log`, `logs/test-classic-108.log`, `logs/test-player-108.log`, `logs/test-navigation-108.log`, `logs/test-keyboard-108.log` and `logs/results-108.json`.\n- `build/classic-movies-108.png`, `build/classic-seasons-108.png` and `build/classic-episodes-108.png` are host previews using test metadata and public poster images; they are not PS4 captures.\n',1)
f.write_text(t,newline='\n')
print(json.dumps(result,indent=2))
