from pathlib import Path
import csv,hashlib,json
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
assert all(hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest().upper()==row['Hash'] for row in rows)
assert hashlib.sha256(Path(r'D:\Coding-Projects\PS3\JellyFin-PS3-LosslessAudio\ICON0.PNG').read_bytes()).digest()==hashlib.sha256((p/'vendor/ps3/ICON0.PNG').read_bytes()).digest()
previous={'dist/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'61F9A12B642DC3E4798DAC1220F6112DA94361F32312706153788E0C54A95B81','dist/v1.01/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'CB045BCFF2D2C7A980A58572850AF65F7AD1B412085A9805FF52D1B3F22260DA','dist/v1.02/IV0000-JFPS40000_00-JELLYFINPS400000.pkg':'7801BFA3C16A8EE3E34BA98121B9A455031CBE645E213A16DF95E8F7C05F4D2B'}
for path,digest in previous.items():assert hashlib.sha256((p/path).read_bytes()).hexdigest().upper()==digest
pkg=p/'dist/v1.03/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
results={'version':'01.03','title':'Jellyfin [PS]','platform':'PS4','pkg':str(pkg.relative_to(p)),'bytes':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_checks_passed':28,'module_loader_tests':'passed: cached, public success, public failure/kernel fallback, complete failure, startup failure, required/optional API handling','ui_previews':'shared renderer, publicly readable artwork from connected library, host-only fixture metadata','icon_validation':'pixel comparison against proportionally resized original PS3 icon passed','screenshot_validation':'BMP colour and row orientation passed','console_capture':'R3, /data/JellyFin-PS4-logs/screenshots/*.bmp','hardware_decoder_retest':'required','ps3_hashes_unchanged':len(rows),'previous_pkgs_unchanged':True,'console_operations':'read-only FTP log retrieval and module filename listings; no uploads/installations/modifications'}
(p/'logs/results-103.json').write_text(json.dumps(results,indent=2))
f=p/'README.md';t=f.read_text();a=t.index('Latest update:');b=t.index('\n\n',a);t=t[:a]+'Latest update: **v1.03**. See [release notes](RELEASE-1.03.md). The new package is in `dist/v1.03/`; all previous releases are preserved.'+t[b:]
t=t.replace('`dist/v1.02/IV0000-JFPS40000_00-JELLYFINPS400000.pkg`','`dist/v1.03/IV0000-JFPS40000_00-JELLYFINPS400000.pkg`').replace('application version `01.02`','application version `01.03`').replace('7801BFA3C16A8EE3E34BA98121B9A455031CBE645E213A16DF95E8F7C05F4D2B',results['sha256'])
import re
t=re.sub(r'- Size: [\d,]+ bytes\.', '- Size: '+format(results['bytes'],',')+' bytes.', t)
t=t.replace('Version 1.02 keyboard and native networking changes still need a console retest.', 'The user confirmed the keyboard, server connection and sign-in work. AvPlayer loading and the new layout in v1.03 still need a console retest.')
t=t.replace('- Native controller-driven library interface,', '- Original PS3 application icon, family title **Jellyfin [PS]**, PS4 label, original brand mark/theme palettes, large artwork cards and separate poster/detail controls.\n- Native controller-driven library interface,')
t=t.replace('- PS4 AvPlayer adapter with', '- AvPlayer module lookup/reuse, public-loader attempt and kernel-loader fallback using the existing on-console module; exact load/start/export failure diagnostics and optional basic-frame fallback.\n- PS4 AvPlayer adapter with')
t=t.replace('## Local verification\n', '## Local verification\n\n- `logs/build-103-release.log`: v1.03 build and 28 passing PKG checks.\n- `logs/ui-loader-tests-103-final.log`: module-loader recovery/failure tests and shared UI/screenshot renderer checks.\n- `build/library-preview-103.png` and `build/detail-preview-103.png`: locally rendered layouts using public server artwork and host-only test metadata, not PS4 captures.\n- Press **R3** inside the new app to save the actual PS4 frame to `/data/JellyFin-PS4-logs/screenshots/`. The FTP retrieval script downloads recent captures read-only. The BMP encoder reuses the PS3 screenshot implementation; filenames are unique and do not overwrite older captures.\n')
t=t.replace('From PowerShell in this directory:', 'From PowerShell in this directory (Windows Python/Pillow prepares the original PS3 image assets before compilation):')
f.write_text(t,newline='\n')
print(json.dumps(results,indent=2))
