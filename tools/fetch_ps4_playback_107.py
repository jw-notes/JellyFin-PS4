import json,urllib.request,tarfile,hashlib
from pathlib import Path
root=Path('tools/ps4-portlibs-107');root.mkdir(exist_ok=True)
assets=json.loads(Path('logs/switchfin-reference-107/pacbrew-assets.json').read_text())['assets']
want=['libmpv-0.36.0-3','ffmpeg-7.1.5-4','mbedtls-2.28.10-2','dav1d-1.5.4-1','sdl2-2.0.18-19']
for asset in assets:
 if not any(x in asset['name'] for x in want):continue
 archive=root/asset['name']
 if not archive.exists():urllib.request.urlretrieve(asset['browser_download_url'],archive)
 print(asset['name'],hashlib.sha256(archive.read_bytes()).hexdigest())
 with tarfile.open(archive) as tf:
  for member in tf.getmembers():
   if member.name.startswith('opt/pacbrew/ps4/openorbis/') and not member.issym() and not member.islnk():
    target=(root/member.name).resolve();assert target.is_relative_to(root.resolve());tf.extract(member,root,filter='data')
for path in ['scripts/ps4/ffmpeg/videodec2.patch','scripts/ps4/libmpv/mpv.patch']:
 (Path('logs/switchfin-reference-107')/('ps4-branch-'+path.replace('/','_'))).write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/dragonflylee/switchfin/ps4/'+path,timeout=30).read())
print('PS4 playback packages extracted locally')
