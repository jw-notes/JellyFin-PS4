from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tools/setup_live_test_104.py';t=f.read_text().replace("existing=req('/LiveTv/TunerHosts')", "existing=req('/System/Configuration/livetv').get('TunerHosts',[])");f.write_text(t,newline='\n')
