from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tools/package.py';t=f.read_text().replace("'TITLE':'JellyFin [PS4]'","'TITLE':'Jellyfin [PS]'");a=t.index('# Resize the original');b=t.index("(pkg/'sce_sys/about')",a);t=t[:a]+'''# Packaging must always use the original PS3 icon; never substitute artwork.
shutil.copyfile(p/'assets/icon0.png',pkg/'sce_sys/icon0.png')
(pkg/'assets').mkdir(exist_ok=True)
shutil.copyfile(p/'assets/jellyfin-mark.png',pkg/'assets/jellyfin-mark.png')

'''+t[b:];f.write_text(t,newline='\n')
f=p/'tools/build.ps1';t=f.read_text().replace("$linuxRoot =", "& python (Join-Path $PSScriptRoot 'prepare_assets.py')\nif ($LASTEXITCODE -ne 0) { throw 'Original icon asset preparation failed' }\n$linuxRoot =");f.write_text(t,newline='\n')
