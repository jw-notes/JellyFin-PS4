from pathlib import Path
import csv,hashlib,json
p=Path(__file__).resolve().parent.parent
rows=list(csv.DictReader((p/'logs/ps3-source-hashes.csv').open(encoding='utf-8-sig')))
changed=[]
for row in rows:
 f=Path(row['Path'])
 if hashlib.sha256(f.read_bytes()).hexdigest().upper()!=row['Hash']:changed.append(str(f))
report={'files_checked':len(rows),'changed_files':changed,'original_source_unchanged':not changed}
(p/'logs/source-preservation-audit.json').write_text(json.dumps(report,indent=2))
assert not changed,changed
print('PASS original PS3 source preservation:',len(rows),'file hashes unchanged')
archives={}
for f in (p/'tools').glob('*.tar.gz'):archives[f.name]={'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
(p/'logs/toolchain-downloads.json').write_text(json.dumps({'openorbis_release':'v0.5.4','url':'https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz','llvm':'18.1.8','archives':archives},indent=2))
