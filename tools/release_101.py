from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tools/build.sh';t=f.read_text().replace('source/core.cpp source/render.cpp','source/core.cpp source/auth.cpp source/login_ui.cpp source/render.cpp').replace('-lSceAudioOut -o','-lSceAudioOut -lSceSystemService -o')
t=t.replace('SDK="$OO_PS4_TOOLCHAIN"','SDK="$OO_PS4_TOOLCHAIN"\nRELEASE_DIR="dist/$(python3 -c \'import json;print(json.load(open("version.json"))["release_directory"])\')"\nmkdir -p "$RELEASE_DIR"')
t=t.replace('pkg_build project.gp4 ../../dist','pkg_build project.gp4 "../../$RELEASE_DIR"');f.write_text(t,newline='\n')
f=p/'tools/package.py';t=f.read_text().replace('import subprocess, shutil, os, re, struct, zlib','import subprocess, shutil, os, re, struct, zlib, json').replace("values={'APP_TYPE':1,'APP_VER':'01.00'", "version=json.loads((p/'version.json').read_text(encoding='utf-8-sig'))['app_version']\nvalues={'APP_TYPE':1,'APP_VER':version").replace("'VERSION':'01.00'", "'VERSION':version");f.write_text(t,newline='\n')
f=p/'tools/build.ps1';t=f.read_text().replace("$pkg = Join-Path $projectRoot 'dist\\IV0000-JFPS40000_00-JELLYFINPS400000.pkg'", "$version = Get-Content (Join-Path $projectRoot 'version.json') -Raw | ConvertFrom-Json\n$pkg = Join-Path $projectRoot ('dist\\' + $version.release_directory + '\\IV0000-JFPS40000_00-JELLYFINPS400000.pkg')");f.write_text(t,newline='\n')
f=p/'tools/test.sh';t=f.read_text().replace('source/app.cpp source/core.cpp','source/app.cpp source/auth.cpp source/core.cpp');f.write_text(t,newline='\n')
f=p/'tools/test_renderer.sh';t=f.read_text().replace('source/app.cpp source/core.cpp','source/app.cpp source/auth.cpp source/core.cpp');f.write_text(t,newline='\n')
