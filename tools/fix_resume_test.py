from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tools/setup_test_server.py';t=f.read_text().replace("token=session['AccessToken']", "token=session['AccessToken']\nconfig=req('/System/Configuration',token=token)\nconfig['MinResumeDurationSeconds']=60\nreq('/System/Configuration',config,token)");f.write_text(t)
f=p/'source/main.cpp';t=f.read_text().replace('time_t began=time(nullptr),last_report=began;', 'time_t began=time(nullptr),last_report=began;bool reported=false;').replace('if(started&&last_report==began){','if(started&&!reported){reported=true;').replace('last_report=time(nullptr)+1;','last_report=time(nullptr);');f.write_text(t)
