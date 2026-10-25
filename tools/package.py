from pathlib import Path
import subprocess, shutil, os, re, struct, zlib, json
p=Path(__file__).resolve().parent.parent
sdk=p/'tools/OpenOrbis/PS4Toolchain'
pkg=p/'build/pkg'
tool=sdk/'bin/linux/PkgTool.Core'
sfo=pkg/'sce_sys/param.sfo'
subprocess.run([str(tool),'sfo_new',str(sfo)],check=True)
version=json.loads((p/'version.json').read_text(encoding='utf-8-sig'))['app_version']
values={'APP_TYPE':1,'APP_VER':version,'ATTRIBUTE':0,'CATEGORY':'gd','CONTENT_ID':'IV0000-JFPS40000_00-JELLYFINPS400000','DOWNLOAD_DATA_SIZE':0,'SYSTEM_VER':0,'TITLE':'Jellyfin [PS]','TITLE_ID':'JFPS40000','VERSION':version}
for key,value in values.items():
 n=4 if isinstance(value,int) else {'APP_VER':8,'CATEGORY':4,'CONTENT_ID':48,'TITLE':128,'TITLE_ID':12,'VERSION':8}[key]
 subprocess.run([str(tool),'sfo_setentry',str(sfo),key,'--type','Integer' if isinstance(value,int) else 'Utf8','--maxsize',str(n),'--value',str(value)],check=True,stdout=subprocess.DEVNULL)
for file in (p/'vendor/ps3/themes').rglob('*.ini'):shutil.copyfile(file,pkg/'themes'/file.name)
# Packaging must always use the original PS3 icon; never substitute artwork.
shutil.copyfile(p/'assets/icon0.png',pkg/'sce_sys/icon0.png')
(pkg/'assets').mkdir(exist_ok=True)
shutil.copyfile(p/'assets/jellyfin-mark.png',pkg/'assets/jellyfin-mark.png')
shutil.copytree(p/'assets/prompts',pkg/'assets/prompts',dirs_exist_ok=True)

(pkg/'sce_sys/about').mkdir(exist_ok=True)
(pkg/'sce_module').mkdir(exist_ok=True)
for name,dest in [('right.sprx','sce_sys/about/right.sprx'),('libc.prx','sce_module/libc.prx'),('libSceFios2.prx','sce_module/libSceFios2.prx')]:
 shutil.copyfile(sdk/'src/modules'/name,pkg/dest)
