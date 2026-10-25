from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/app.cpp';t=f.read_text().replace('void plog(const char* msg){FILE* f=', 'void plog(const char* msg){if(data_dir.empty())return;FILE* f=');f.write_text(t,newline='\n')
