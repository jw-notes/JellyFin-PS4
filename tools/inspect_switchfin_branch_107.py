from pathlib import Path
import json,urllib.request
p=Path('logs/switchfin-reference-107');branch='ps4'
tree=json.load(urllib.request.urlopen('https://api.github.com/repos/dragonflylee/switchfin/git/trees/'+branch+'?recursive=1',timeout=25));(p/'ps4-tree.json').write_text(json.dumps(tree))
for path in ['app/src/view/mpv_core.cpp','app/include/view/mpv_core.hpp','CMakeLists.txt','scripts/ps4/ffmpeg/PKGBUILD','scripts/ps4/libmpv/PKGBUILD','.gitmodules']:
 try:(p/('ps4-branch-'+path.replace('/','_'))).write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/dragonflylee/switchfin/'+branch+'/'+path,timeout=20).read())
 except Exception as e:print(path,type(e).__name__)
release=json.load(urllib.request.urlopen('https://api.github.com/repos/dragonflylee/switchfin/releases/tags/pacbrew',timeout=25));(p/'pacbrew-assets.json').write_text(json.dumps(release))
for asset in release['assets']:print(asset['name'],asset['size'],asset['browser_download_url'])
