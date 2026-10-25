from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
for f in [p/'tools/build.sh',p/'tools/package.py',p/'build/pkg/project.gp4']:
 t=f.read_text().replace('JELLYFINPS4000000','JELLYFINPS400000');f.write_text(t,newline='\n')
f=p/'tools/package.py';t=f.read_text();t+='''\n(pkg/'sce_sys/about').mkdir(exist_ok=True)
(pkg/'sce_module').mkdir(exist_ok=True)
for name,dest in [('right.sprx','sce_sys/about/right.sprx'),('libc.prx','sce_module/libc.prx'),('libSceFios2.prx','sce_module/libSceFios2.prx')]:
 shutil.copyfile(sdk/'src/modules'/name,pkg/dest)
''';f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text();t=t.replace('"$SDK/bin/linux/PkgTool.Core" pkg_build project.gp4 ../../dist','''cd ../..
python3 tools/fix_gp4.py
cd build/pkg
if [[ -f /proc/sys/fs/binfmt_misc/WSLInterop ]]; then
 DOTNET_ROLL_FORWARD=LatestMajor "$SDK/bin/windows/PkgTool.Core.exe" pkg_build project.gp4 ../../dist
else
 "$SDK/bin/linux/PkgTool.Core" pkg_build project.gp4 ../../dist
fi''');f.write_text(t,newline='\n')
