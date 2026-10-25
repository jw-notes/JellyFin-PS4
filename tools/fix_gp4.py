from pathlib import Path
import xml.etree.ElementTree as E
p=Path(__file__).resolve().parent.parent/'build/pkg/project.gp4'
t=E.parse(p);r=t.getroot().find('rootdir');r.clear()
files=t.getroot().find('files')
for file in files:
 parts=Path(file.attrib['targ_path']).parts[:-1]
 current=r
 for part in parts:
  child=next((v for v in current if v.attrib.get('targ_name')==part),None)
  if child is None:child=E.SubElement(current,'dir',{'targ_name':part})
  current=child
E.indent(t);t.write(p,encoding='utf-8',xml_declaration=True)
