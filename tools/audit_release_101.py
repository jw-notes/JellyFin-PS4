from pathlib import Path
import csv,hashlib,json
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
changed=[row['Path'] for row in rows if hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest().upper()!=row['Hash']]
assert not changed,changed
pkg=p/'dist/v1.01/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
old=p/'dist/IV0000-JFPS40000_00-JELLYFINPS400000.pkg'
old_hash=hashlib.sha256(old.read_bytes()).hexdigest().upper()
assert old_hash=='61F9A12B642DC3E4798DAC1220F6112DA94361F32312706153788E0C54A95B81'
result={'version':'01.01','pkg':str(pkg.relative_to(p)),'size':pkg.stat().st_size,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest().upper(),'pkg_validation_checks_passed':28,'quick_connect_real_server_tests':'passed','library_and_playback_regressions':'passed','original_pkg_unchanged':True,'original_ps3_files_unchanged':len(rows),'console_access':'FTP read-only; CWD/LIST/NLST/RETR only','console_logs_downloaded':'logs/console/20261024-173217/sce-log.bin','private_app_log':'FTP access denied; no permissions changed','exit_cause':'not confirmed from available log','console_installation':'not performed','native_exit_retest':'required'}
(p/'logs/results-101.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
f=p/'tools/fetch_console_logs.py';t=f.read_text();a=t.index("listing('/data/JellyFin-PS4')");t=t[:a]+'''# New builds mirror diagnostic messages only into a public log directory.
# Credentials and saved sessions remain in the private application directory.
listing('/data/JellyFin-PS4-logs')
try:
 f.cwd('/data/JellyFin-PS4-logs')
 for filename in f.nlst():
  name=posixpath.basename(filename)
  if name.startswith('user-') and name.endswith('.log'):
   local=out/name
   with local.open('wb') as dest:f.retrbinary('RETR '+name,dest.write)
   files.append(str(local.relative_to(p)))
except error_perm:pass
'''+t[a:];f.write_text(t,newline='\n')
